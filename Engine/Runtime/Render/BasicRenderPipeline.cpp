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

#include "Render/BasicRenderPipeline.h"

#include "GPU/GPUConstantPool.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUPipeline.h"

#include "Render/EntityDrawList.h"
#include "Render/RenderContext.h"
#include "Render/RenderWorld.h"
#include "Render/ShaderManager.h"

class BasicRenderContext : public RenderContext
{
public:
    using RenderContext::RenderContext;

    CullResults                 cullResults;
    EntityDrawList              drawList;
};

BasicRenderPipeline::BasicRenderPipeline() :
    clearColour (0.0f, 0.0f, 0.0f, 1.0f)
{
    // Temporary.
    mVertexShader = ShaderManager::Get().GetShader("Engine/BasicRenderPipeline.hlsl", "VSMain", kGPUShaderStage_Vertex);
    mPixelShader  = ShaderManager::Get().GetShader("Engine/BasicRenderPipeline.hlsl", "PSMain", kGPUShaderStage_Pixel);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(1);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Constants;

    mArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));
    mArgumentSet       = GPUDevice::Get().CreateArgumentSet(mArgumentSetLayout, nullptr);
}

BasicRenderPipeline::~BasicRenderPipeline()
{
    delete mArgumentSet;
}

void BasicRenderPipeline::Render(const RenderWorld&         inWorld,
                                 const RenderView&          inView,
                                 RenderGraph&               inGraph,
                                 const RenderResourceHandle inTexture,
                                 RenderResourceHandle&      outNewTexture)
{
    BasicRenderContext* const context = inGraph.NewTransient<BasicRenderContext>(inWorld, inView);

    /* Get the visible entities. */
    inWorld.Cull(inView, context->cullResults);

    /* Build a draw list for the entities. */
    context->drawList.Reserve(context->cullResults.entities.size());
    for (const RenderEntity* entity : context->cullResults.entities)
    {
        // TODO: Materials, move all this to the entity (GetDrawCall()).

        const glm::mat4 mvpMatrix    = inView.GetViewProjectionMatrix() * entity->GetTransform().GetMatrix();
        const GPUConstants constants = GPUDevice::Get().GetConstantPool().Write(&mvpMatrix, sizeof(mvpMatrix));

        const RenderTextureDesc& textureDesc = inGraph.GetTextureDesc(inTexture);

        // To pre-create the PSO in the entity we will need to have the render
        // target state/formats known somewhere - they will be defined for the
        // ShaderPassType that the PSO is for.
        GPURenderTargetStateDesc renderTargetDesc;
        renderTargetDesc.colour[0]    = textureDesc.format;
        renderTargetDesc.depthStencil = kPixelFormat_Depth32;

        GPUDepthStencilStateDesc depthDesc;
        depthDesc.depthTestEnable  = true;
        depthDesc.depthWriteEnable = true;
        depthDesc.depthCompareOp   = kGPUCompareOp_LessOrEqual;

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex] = mVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]  = mPixelShader;
        pipelineDesc.argumentSetLayouts[0]           = mArgumentSetLayout;
        pipelineDesc.blendState                      = GPUBlendState::Get(GPUBlendStateDesc());
        pipelineDesc.depthStencilState               = GPUDepthStencilState::Get(depthDesc);
        pipelineDesc.rasterizerState                 = GPURasterizerState::Get(GPURasterizerStateDesc());
        pipelineDesc.renderTargetState               = GPURenderTargetState::Get(renderTargetDesc);
        pipelineDesc.vertexInputState                = entity->GetVertexInputState();
        pipelineDesc.topology                        = kGPUPrimitiveTopology_TriangleList;

        GPUPipeline* const pipeline = GPUDevice::Get().GetPipeline(pipelineDesc);

        const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
        EntityDrawCall& drawCall = context->drawList.Add(sortKey);

        drawCall.pipeline = pipeline;

        drawCall.arguments[0].argumentSet                = mArgumentSet;
        drawCall.arguments[0].constants[0].argumentIndex = 0;
        drawCall.arguments[0].constants[0].constants     = constants;

        entity->GetGeometry(drawCall);
    }

    /* Sort them. */
    context->drawList.Sort();

    /* Add the main pass. */
    RenderGraphPass& pass = inGraph.AddPass("Scene", kRenderGraphPassType_Render);

    RenderTextureDesc depthTextureDesc(inGraph.GetTextureDesc(inTexture));
    depthTextureDesc.format = kPixelFormat_Depth32;

    RenderResourceHandle depthTexture = inGraph.CreateTexture(depthTextureDesc);

    pass.SetColour(0, inTexture, &outNewTexture);
    pass.SetDepthStencil(depthTexture, kGPUResourceState_DepthStencilWrite);

    pass.ClearColour(0, this->clearColour);
    pass.ClearDepth(1.0f);

    pass.SetFunction([context] (const RenderGraph&      inGraph,
                                const RenderGraphPass&  inPass,
                                GPUGraphicsCommandList& inCmdList)
    {
        context->drawList.Draw(inCmdList);

        // debug renderer bounding boxes
    });
}
