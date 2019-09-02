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
    mPasses {}
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

void ShaderTechnique::Deserialise(Serialiser& inSerialiser)
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

            pass->mShaders[stage] = ShaderManager::Get().GetShader(source, function, stage);

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
