/*
 * Copyright (C) 2018-2020 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "Render/ShaderTechnique.h"

#include "Engine/AssetManager.h"
#include "Engine/Serialiser.h"
#include "Engine/Texture.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUDevice.h"

#include "Render/BasicRenderPipeline.h"
#include "Render/DeferredRenderPipeline.h"
#include "Render/ShaderManager.h"

static void GetDefaultStates(const ShaderPassType      passType,
                             GPUBlendStateDesc&        outBlendDesc,
                             GPUDepthStencilStateDesc& outDepthStencilDesc,
                             GPURasterizerStateDesc&   outRasterizerDesc,
                             GPURenderTargetStateDesc& outRenderTargetDesc)
{
    switch (passType)
    {
        case kShaderPassType_Basic:
        {
            outRenderTargetDesc.colour[0]    = BasicRenderPipeline::kColourFormat;
            outRenderTargetDesc.depthStencil = BasicRenderPipeline::kDepthFormat;

            outDepthStencilDesc.depthTestEnable  = true;
            outDepthStencilDesc.depthWriteEnable = true;
            outDepthStencilDesc.depthCompareOp   = kGPUCompareOp_LessOrEqual;

            break;
        }

        case kShaderPassType_DeferredOpaque:
        {
            outRenderTargetDesc.colour[0]    = DeferredRenderPipeline::kGBuffer0Format;
            outRenderTargetDesc.colour[1]    = DeferredRenderPipeline::kGBuffer1Format;
            outRenderTargetDesc.colour[2]    = DeferredRenderPipeline::kGBuffer2Format;
            outRenderTargetDesc.colour[3]    = DeferredRenderPipeline::kColourFormat;
            outRenderTargetDesc.depthStencil = DeferredRenderPipeline::kDepthFormat;

            outDepthStencilDesc.depthTestEnable  = true;
            outDepthStencilDesc.depthWriteEnable = true;
            outDepthStencilDesc.depthCompareOp   = kGPUCompareOp_LessOrEqual;

            break;
        }

        case kShaderPassType_ShadowMap:
        {
            outRenderTargetDesc.depthStencil = RenderPipeline::kShadowMapFormat;

            outDepthStencilDesc.depthTestEnable  = true;
            outDepthStencilDesc.depthWriteEnable = true;
            outDepthStencilDesc.depthCompareOp   = kGPUCompareOp_LessOrEqual;

            break;
        }

        default:
        {
            UnreachableMsg("Unknown pass type %d", passType);
            break;
        }
    }
}


ShaderPass::ShaderPass() :
    mBlendState         (nullptr),
    mDepthStencilState  (nullptr),
    mRasterizerState    (nullptr),
    mRenderTargetState  (nullptr)
{
}

ShaderPass::~ShaderPass()
{
}

ShaderTechnique::ShaderTechnique() :
    mPasses             {},
    mArgumentSetLayout  (nullptr),
    mConstantsSize      (0),
    mConstantsIndex     (std::numeric_limits<uint32_t>::max())
{
}

ShaderTechnique::~ShaderTechnique()
{
    for (auto pass : mPasses)
    {
        delete pass;
    }
}

void ShaderTechnique::Serialise(Serialiser& serialiser) const
{
    LogError("TODO");
}

void ShaderTechnique::DeserialiseParameters(Serialiser& serialiser)
{
    bool found;

    GPUArgumentSetLayoutDesc layoutDesc;

    found = serialiser.BeginArray("parameters");
    if (found)
    {
        while (serialiser.BeginGroup())
        {
            mParameters.emplace_back();
            ShaderParameter& parameter = mParameters.back();

            found = serialiser.Read("name", parameter.name);
            Assert(found);

            found = serialiser.Read("type", parameter.type);
            Assert(found);
            Assert(parameter.type < kShaderParameterTypeCount);

            if (ShaderParameter::IsResource(parameter.type))
            {
                parameter.argumentIndex = layoutDesc.arguments.size();
                layoutDesc.arguments.emplace_back(ShaderParameter::GetGPUArgumentType(parameter.type));

                if (ShaderParameter::HasSampler(parameter.type))
                {
                    /* Samplers go immediately after the main resource. */
                    layoutDesc.arguments.emplace_back(kGPUArgumentType_Sampler);
                }

                ObjPtr<> resource;
                switch (parameter.type)
                {
                    case kShaderParameterType_Texture2D:
                    {
                        Texture2DPtr texture;
                        if (!serialiser.Read("default", texture))
                        {
                            texture = AssetManager::Get().Load<Texture2D>("Engine/Textures/DummyBlack2D");
                        }

                        resource = std::move(texture);
                        break;
                    }

                    case kShaderParameterType_TextureCube:
                    {
                        TextureCubePtr texture;
                        if (!serialiser.Read("default", texture))
                        {
                            texture = AssetManager::Get().Load<TextureCube>("Engine/Textures/DummyBlackCube");
                        }

                        resource = std::move(texture);
                        break;
                    }

                    default:
                    {
                        UnreachableMsg("Unhandled parameter type %d", parameter.type);
                        break;
                    }
                }

                Assert(resource);

                mDefaultResources.resize(parameter.argumentIndex + 1);
                mDefaultResources[parameter.argumentIndex] = std::move(resource);
            }
            else
            {
                const uint32_t size = ShaderParameter::GetSize(parameter.type);

                /* Respect HLSL packing rules. A constant buffer member will
                 * not straddle a 16-byte boundary. */
                if ((mConstantsSize / 16) != ((mConstantsSize + size - 1) / 16))
                {
                    mConstantsSize = RoundUpPow2(mConstantsSize, 16u);
                }

                parameter.constantOffset = mConstantsSize;

                mConstantsSize += size;

                /* Read default values. Zero-initialise if we don't have a
                 * default specified. */
                mDefaultConstantData.Resize(mConstantsSize);

                #define READ_TYPE(typeEnum, typeName) \
                    case typeEnum: \
                    { \
                        typeName value{}; \
                        serialiser.Read("default", value); \
                        memcpy(mDefaultConstantData.Get() + parameter.constantOffset, &value, size); \
                        break; \
                    }

                switch (parameter.type)
                {
                    READ_TYPE(kShaderParameterType_Int,    int32_t);
                    READ_TYPE(kShaderParameterType_Int2,   glm::ivec2);
                    READ_TYPE(kShaderParameterType_Int3,   glm::ivec3);
                    READ_TYPE(kShaderParameterType_Int4,   glm::ivec4);
                    READ_TYPE(kShaderParameterType_UInt,   uint32_t);
                    READ_TYPE(kShaderParameterType_UInt2,  glm::uvec2);
                    READ_TYPE(kShaderParameterType_UInt3,  glm::uvec3);
                    READ_TYPE(kShaderParameterType_UInt4,  glm::uvec4);
                    READ_TYPE(kShaderParameterType_Float,  float);
                    READ_TYPE(kShaderParameterType_Float2, glm::vec2);
                    READ_TYPE(kShaderParameterType_Float3, glm::vec3);
                    READ_TYPE(kShaderParameterType_Float4, glm::vec4);

                    default:
                        UnreachableMsg("Unhandled parameter type %d", parameter.type);
                        break;

                }

                #undef READ_TYPE
            }

            serialiser.EndGroup();
        }

        serialiser.EndArray();
    }

    if (mConstantsSize)
    {
        mConstantsIndex = layoutDesc.arguments.size();
        layoutDesc.arguments.emplace_back(kGPUArgumentType_Constants);
    }

    if (!layoutDesc.arguments.empty())
    {
        mArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(layoutDesc));
    }
}

void ShaderTechnique::DeserialisePasses(Serialiser& serialiser)
{
    bool found;

    found = serialiser.BeginArray("passes");
    Assert(found);

    while (serialiser.BeginGroup())
    {
        ShaderPassType passType;
        found = serialiser.Read("type", passType);
        Assert(found);
        Assert(passType < kShaderPassTypeCount);

        auto pass = new ShaderPass();
        mPasses[passType] = pass;

        found = serialiser.BeginArray("shaders");
        Assert(found);

        while (serialiser.BeginGroup())
        {
            GPUShaderStage stage;
            found = serialiser.Read("stage", stage);
            Assert(found);
            Assert(stage < kGPUShaderStage_NumGraphics);

            std::string source;
            found = serialiser.Read("source", source);
            Assert(found);

            std::string function;
            found = serialiser.Read("function", function);
            Assert(found);

            pass->mShaders[stage] = ShaderManager::Get().GetShader(source, function, stage, this);

            serialiser.EndGroup();
        }

        serialiser.EndArray();

        GPUBlendStateDesc blendDesc;
        GPUDepthStencilStateDesc depthStencilDesc;
        GPURasterizerStateDesc rasterizerDesc;
        GPURenderTargetStateDesc renderTargetDesc;

        GetDefaultStates(passType,
                         blendDesc,
                         depthStencilDesc,
                         rasterizerDesc,
                         renderTargetDesc);

        /* TODO: Allow overriding some states in the asset. */

        pass->mBlendState        = GPUBlendState::Get(blendDesc);
        pass->mDepthStencilState = GPUDepthStencilState::Get(depthStencilDesc);
        pass->mRasterizerState   = GPURasterizerState::Get(rasterizerDesc);
        pass->mRenderTargetState = GPURenderTargetState::Get(renderTargetDesc);

        serialiser.EndGroup();
    }

    serialiser.EndArray();
}

void ShaderTechnique::Deserialise(Serialiser& serialiser)
{
    DeserialiseParameters(serialiser);
    DeserialisePasses(serialiser);
}

const ShaderParameter* ShaderTechnique::FindParameter(const std::string& name) const
{
    /* If this ever becomes a bottleneck try a map? The number of parameters
     * for a technique will typically be small though so I don't think it's
     * worth it for now. */
    for (const ShaderParameter& parameter : mParameters)
    {
        if (parameter.name == name)
        {
            return &parameter;
        }
    }

    return nullptr;
}
