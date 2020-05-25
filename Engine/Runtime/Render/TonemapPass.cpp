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

#include "Render/TonemapPass.h"

#include "GPU/GPUCommandList.h"
#include "GPU/GPUDevice.h"

#include "Render/RenderGraph.h"
#include "Render/ShaderManager.h"

#include "../../Shaders/Tonemap.h"

TonemapPass::TonemapPass()
{
    mVertexShader = ShaderManager::Get().GetShader("Engine/Tonemap.hlsl", "VSFullScreen", kGPUShaderStage_Vertex);
    mPixelShader  = ShaderManager::Get().GetShader("Engine/Tonemap.hlsl", "PSMain", kGPUShaderStage_Pixel);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(kTonemapArgumentsCount);
    argumentLayoutDesc.arguments[kTonemapArguments_SourceTexture] = kGPUArgumentType_Texture;

    mArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::AddPass(RenderGraph&               graph,
                          const RenderResourceHandle sourceTexture,
                          RenderResourceHandle&      ioDestTexture) const
{
    RenderGraphPass& pass = graph.AddPass("Tonemap", kRenderGraphPassType_Render);

    RenderViewDesc viewDesc;
    viewDesc.type  = kGPUResourceViewType_Texture2D;
    viewDesc.state = kGPUResourceState_PixelShaderRead;

    const RenderViewHandle viewHandle = pass.CreateView(sourceTexture, viewDesc);

    pass.SetColour(0, ioDestTexture, &ioDestTexture);

    pass.SetFunction([this, viewHandle] (const RenderGraph&      graph,
                                         const RenderGraphPass&  pass,
                                         GPUGraphicsCommandList& cmdList)
    {
        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]          = mVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]           = mPixelShader;
        pipelineDesc.argumentSetLayouts[kArgumentSet_Tonemap] = mArgumentSetLayout;
        pipelineDesc.blendState                               = GPUBlendState::GetDefault();
        pipelineDesc.depthStencilState                        = GPUDepthStencilState::GetDefault();
        pipelineDesc.rasterizerState                          = GPURasterizerState::GetDefault();
        pipelineDesc.renderTargetState                        = cmdList.GetRenderTargetState();
        pipelineDesc.vertexInputState                         = GPUVertexInputState::GetDefault();
        pipelineDesc.topology                                 = kGPUPrimitiveTopology_TriangleList;

        cmdList.SetPipeline(pipelineDesc);

        GPUArgument arguments[kTonemapArgumentsCount];
        arguments[kTonemapArguments_SourceTexture].view = pass.GetView(viewHandle);

        cmdList.SetArguments(kArgumentSet_Tonemap, arguments);

        cmdList.Draw(3);
    });
}
