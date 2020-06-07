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

static void GetPassStates(const ShaderPassType      passType,
                          const ShaderPassFlags     passFlags,
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

            /* For non-emissive materials, we mask the emissive output. */
            if (!(passFlags & kShaderPassFlags_DeferredOpaque_Emissive))
            {
                outBlendDesc.attachments[3].maskR = true;
                outBlendDesc.attachments[3].maskG = true;
                outBlendDesc.attachments[3].maskB = true;
                outBlendDesc.attachments[3].maskA = true;
            }

            break;
        }

        case kShaderPassType_DeferredUnlit:
        {
            outRenderTargetDesc.colour[0]    = DeferredRenderPipeline::kColourFormat;
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

ShaderTechnique::ShaderTechnique() :
    mPasses             {},
    mArgumentSetLayout  (nullptr),
    mConstantsSize      (0),
    mConstantsIndex     (std::numeric_limits<uint32_t>::max())
{
}

ShaderTechnique::~ShaderTechnique()
{
    for (Pass* pass : mPasses)
    {
        if (pass)
        {
            for (ShaderVariant* variant : pass->variants)
            {
                delete variant;
            }

            delete pass;
        }
    }
}

void ShaderTechnique::Serialise(Serialiser& serialiser) const
{
    LogError("TODO");
}

void ShaderTechnique::DeserialiseFeatures(Serialiser& serialiser)
{
    if (serialiser.BeginArray("features"))
    {
        std::string name;
        while (serialiser.Pop(name))
        {
            mFeatures.emplace_back(std::move(name));
        }

        serialiser.EndArray();
    }
}

uint32_t ShaderTechnique::ConvertFeatureArray(const FeatureArray& features) const
{
    uint32_t mask = 0;

    for (const std::string& feature : features)
    {
        uint32_t index;
        const bool found = FindFeature(feature, index);
        AssertMsg(found, "Feature '%s' not found", feature.c_str());
        Unused(found);

        mask |= 1 << index;
    }

    return mask;
}

uint32_t ShaderTechnique::DeserialiseFeatureArray(Serialiser&       serialiser,
                                                  const char* const name) const
{
    uint32_t mask = 0;

    if (serialiser.BeginArray(name))
    {
        std::string feature;
        while (serialiser.Pop(feature))
        {
            uint32_t index;
            const bool found = FindFeature(feature, index);
            AssertMsg(found, "Feature '%s' not found", feature.c_str());
            Unused(found);

            mask |= 1 << index;
        }

        serialiser.EndArray();
    }

    return mask;
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

            parameter.requires = DeserialiseFeatureArray(serialiser, "requires");

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

        auto pass = new Pass();
        mPasses[passType] = pass;

        found = serialiser.BeginArray("shaders");
        Assert(found);

        while (serialiser.BeginGroup())
        {
            GPUShaderStage stage;
            found = serialiser.Read("stage", stage);
            Assert(found);
            Assert(stage < kGPUShaderStage_NumGraphics);

            pass->shaders[stage].requires = DeserialiseFeatureArray(serialiser, "requires");

            found = serialiser.Read("source", pass->shaders[stage].source);
            Assert(found);

            found = serialiser.Read("function", pass->shaders[stage].function);
            Assert(found);

            serialiser.EndGroup();
        }

        serialiser.EndArray();

        if (serialiser.BeginArray("variants"))
        {
            while (serialiser.BeginGroup())
            {
                VariantProps& props = pass->variantProps.emplace_back();

                props.requires = DeserialiseFeatureArray(serialiser, "requires");
                props.flags    = kShaderPassFlags_None;

                if (serialiser.BeginArray("flags"))
                {
                    ShaderPassFlags flag;
                    while (serialiser.Pop(flag))
                    {
                        props.flags |= flag;
                    }

                    serialiser.EndArray();
                }

                if (serialiser.BeginArray("defines"))
                {
                    std::string define;
                    while (serialiser.Pop(define))
                    {
                        props.defines.emplace_back(std::move(define));
                    }

                    serialiser.EndArray();
                }

                serialiser.EndGroup();
            }

            serialiser.EndArray();
        }

        serialiser.EndGroup();
    }

    serialiser.EndArray();
}

void ShaderTechnique::Deserialise(Serialiser& serialiser)
{
    DeserialiseFeatures(serialiser);
    DeserialiseParameters(serialiser);
    DeserialisePasses(serialiser);
}

bool ShaderTechnique::FindFeature(const std::string& name,
                                  uint32_t&          outIndex) const
{
    for (size_t i = 0; i < mFeatures.size(); i++)
    {
        if (mFeatures[i] == name)
        {
            outIndex = i;
            return true;
        }
    }

    return false;
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

const ShaderVariant* ShaderTechnique::GetVariant(const ShaderPassType passType,
                                                 const uint32_t       features)
{
    // TODO: Reference counting for variants to destroy variants once they are
    // no longer needed by any loaded materials.

    Pass* const pass = mPasses[passType];

    if (!pass)
    {
        return nullptr;
    }

    /* See if we already have this variant. We're using a simple array here
     * rather than a map since this is only hit at material load time so lookup
     * time isn't critical. */
    for (const ShaderVariant* variant : pass->variants)
    {
        if (variant->mFeatures == features)
        {
            return variant;
        }
    }

    /* Not found, create it. */
    auto variant = pass->variants.emplace_back(new ShaderVariant());
    variant->mFeatures = features;

    /* Combine matching variant properties. */
    ShaderPassFlags passFlags = kShaderPassFlags_None;
    ShaderDefineArray defines;

    for (const VariantProps& props : pass->variantProps)
    {
        if ((features & props.requires) == props.requires)
        {
            passFlags |= props.flags;
            defines.insert(defines.end(), props.defines.begin(), props.defines.end());
        }
    }

    /* Compile shaders which are enabled for this variant. */
    for (uint32_t stage = 0; stage < kGPUShaderStage_NumGraphics; stage++)
    {
        const Shader& shader = pass->shaders[stage];

        if (!shader.source.empty() &&
            (features & shader.requires) == shader.requires)
        {
            variant->mShaders[stage] =
                ShaderManager::Get().GetShader(shader.source,
                                               shader.function,
                                               static_cast<GPUShaderStage>(stage),
                                               defines,
                                               this,
                                               features);
        }
    }

    /* Get GPU states. */
    GPUBlendStateDesc blendDesc;
    GPUDepthStencilStateDesc depthStencilDesc;
    GPURasterizerStateDesc rasterizerDesc;
    GPURenderTargetStateDesc renderTargetDesc;

    GetPassStates(passType,
                  passFlags,
                  blendDesc,
                  depthStencilDesc,
                  rasterizerDesc,
                  renderTargetDesc);

    /* TODO: Allow overriding some states in the asset. */

    variant->mBlendState        = GPUBlendState::Get(blendDesc);
    variant->mDepthStencilState = GPUDepthStencilState::Get(depthStencilDesc);
    variant->mRasterizerState   = GPURasterizerState::Get(rasterizerDesc);
    variant->mRenderTargetState = GPURenderTargetState::Get(renderTargetDesc);

    return variant;
}
