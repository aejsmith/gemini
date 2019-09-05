/*
 * Copyright (C) 2018-2019 Alex Smith
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

#include "Engine/Serialiser.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUDevice.h"

#include "Render/BasicRenderPipeline.h"
#include "Render/ShaderManager.h"

static void GetDefaultStates(const ShaderPassType      inPassType,
                             GPUBlendStateDesc&        outBlendDesc,
                             GPUDepthStencilStateDesc& outDepthStencilDesc,
                             GPURasterizerStateDesc&   outRasterizerDesc,
                             GPURenderTargetStateDesc& outRenderTargetDesc)
{
    switch (inPassType)
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

        default:
        {
            UnreachableMsg("Unknown pass type %d", inPassType);
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

void ShaderTechnique::Serialise(Serialiser& inSerialiser) const
{
    LogError("TODO");
}

void ShaderTechnique::DeserialiseParameters(Serialiser& inSerialiser)
{
    bool found;

    GPUArgumentSetLayoutDesc layoutDesc;

    found = inSerialiser.BeginArray("parameters");
    if (found)
    {
        while (inSerialiser.BeginGroup())
        {
            mParameters.emplace_back();
            ShaderParameter& parameter = mParameters.back();

            found = inSerialiser.Read("name", parameter.name);
            Assert(found);

            found = inSerialiser.Read("type", parameter.type);
            Assert(found);
            Assert(parameter.type < kShaderParameterTypeCount);

            if (ShaderParameter::IsResource(parameter.type))
            {
                parameter.argumentIndex = layoutDesc.arguments.size();
                layoutDesc.arguments.emplace_back(ShaderParameter::GetGPUArgumentType(parameter.type));
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
            }

            inSerialiser.EndGroup();
        }

        inSerialiser.EndArray();
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

void ShaderTechnique::DeserialisePasses(Serialiser& inSerialiser)
{
    bool found;

    found = inSerialiser.BeginArray("passes");
    Assert(found);

    while (inSerialiser.BeginGroup())
    {
        ShaderPassType passType;
        found = inSerialiser.Read("type", passType);
        Assert(found);
        Assert(passType < kShaderPassTypeCount);

        auto pass = new ShaderPass();
        mPasses[passType] = pass;

        found = inSerialiser.BeginArray("shaders");
        Assert(found);

        while (inSerialiser.BeginGroup())
        {
            GPUShaderStage stage;
            found = inSerialiser.Read("stage", stage);
            Assert(found);
            Assert(stage < kGPUShaderStage_NumGraphics);

            std::string source;
            found = inSerialiser.Read("source", source);
            Assert(found);

            std::string function;
            found = inSerialiser.Read("function", function);
            Assert(found);

            pass->mShaders[stage] = ShaderManager::Get().GetShader(source, function, stage, this);

            inSerialiser.EndGroup();
        }

        inSerialiser.EndArray();

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

        inSerialiser.EndGroup();
    }

    inSerialiser.EndArray();
}

void ShaderTechnique::Deserialise(Serialiser& inSerialiser)
{
    DeserialiseParameters(inSerialiser);
    DeserialisePasses(inSerialiser);
}

const ShaderParameter* ShaderTechnique::FindParameter(const std::string& inName) const
{
    /* If this ever becomes a bottleneck try a map? The number of parameters
     * for a technique will typically be small though so I don't think it's
     * worth it for now. */
    for (const ShaderParameter& parameter : mParameters)
    {
        if (parameter.name == inName)
        {
            return &parameter;
        }
    }

    return nullptr;
}
