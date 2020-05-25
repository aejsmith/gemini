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

#include "Render/FXAAPass.h"

#include "GPU/GPUCommandList.h"
#include "GPU/GPUDevice.h"

#include "Render/RenderGraph.h"
#include "Render/ShaderManager.h"

#include "../../Shaders/FXAA.h"

FXAAPass::FXAAPass()
{
    mVertexShader = ShaderManager::Get().GetShader("Engine/FXAA.hlsl", "VSFullScreen", kGPUShaderStage_Vertex);
    mPixelShader  = ShaderManager::Get().GetShader("Engine/FXAA.hlsl", "PSMain", kGPUShaderStage_Pixel);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(kFXAAArgumentsCount);
    argumentLayoutDesc.arguments[kFXAAArguments_SourceTexture] = kGPUArgumentType_Texture;
    argumentLayoutDesc.arguments[kFXAAArguments_SourceSampler] = kGPUArgumentType_Sampler;
    argumentLayoutDesc.arguments[kFXAAArguments_Constants]     = kGPUArgumentType_Constants;

    mArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    GPUSamplerDesc samplerDesc;
    samplerDesc.minFilter = kGPUFilter_Linear;
    samplerDesc.magFilter = kGPUFilter_Linear;

    mSampler = GPUDevice::Get().GetSampler(samplerDesc);
}

FXAAPass::~FXAAPass()
{
}

void FXAAPass::AddPass(RenderGraph&               graph,
                       const RenderResourceHandle sourceTexture,
                       RenderResourceHandle&      ioDestTexture) const
{
    RenderGraphPass& pass = graph.AddPass("FXAA", kRenderGraphPassType_Render);

    RenderViewDesc viewDesc;
    viewDesc.type  = kGPUResourceViewType_Texture2D;
    viewDesc.state = kGPUResourceState_PixelShaderRead;

    const RenderViewHandle viewHandle = pass.CreateView(sourceTexture, viewDesc);

    pass.SetColour(0, ioDestTexture, &ioDestTexture);

    pass.SetFunction([=] (const RenderGraph&      graph,
                          const RenderGraphPass&  pass,
                          GPUGraphicsCommandList& cmdList)
    {
        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]       = mVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]        = mPixelShader;
        pipelineDesc.argumentSetLayouts[kArgumentSet_FXAA] = mArgumentSetLayout;
        pipelineDesc.blendState                            = GPUBlendState::GetDefault();
        pipelineDesc.depthStencilState                     = GPUDepthStencilState::GetDefault();
        pipelineDesc.rasterizerState                       = GPURasterizerState::GetDefault();
        pipelineDesc.renderTargetState                     = cmdList.GetRenderTargetState();
        pipelineDesc.vertexInputState                      = GPUVertexInputState::GetDefault();
        pipelineDesc.topology                              = kGPUPrimitiveTopology_TriangleList;

        cmdList.SetPipeline(pipelineDesc);

        GPUArgument arguments[kFXAAArgumentsCount];
        arguments[kFXAAArguments_SourceTexture].view    = pass.GetView(viewHandle);
        arguments[kFXAAArguments_SourceSampler].sampler = mSampler;

        cmdList.SetArguments(kArgumentSet_FXAA, arguments);

        const RenderTextureDesc& desc = graph.GetTextureDesc(sourceTexture);

        FXAAConstants constants;
        constants.rcpFrame = glm::vec2(1.0f) / glm::vec2(desc.width, desc.height);

        cmdList.WriteConstants(kArgumentSet_FXAA,
                               kFXAAArguments_Constants,
                               constants);

        cmdList.Draw(3);
    });
}
