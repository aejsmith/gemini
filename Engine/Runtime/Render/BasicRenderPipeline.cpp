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
#include "GPU/GPUContext.h"
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
    mVertexShader = ShaderManager::Get().GetShader("Engine/Basic.hlsl", "VSMain", kGPUShaderStage_Vertex);
    mPixelShader  = ShaderManager::Get().GetShader("Engine/Basic.hlsl", "PSMain", kGPUShaderStage_Pixel);

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
        if (entity->SupportsPassType(kShaderPassType_Basic))
        {
            const GPUPipeline* const pipeline = entity->GetPipeline(kShaderPassType_Basic);

            const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
            EntityDrawCall& drawCall = context->drawList.Add(sortKey);

            entity->GetDrawCall(kShaderPassType_Basic, *context, drawCall);

            // TODO: Move argument sets to GetDrawCall(), deduplicate.
            const glm::mat4 mvpMatrix    = inView.GetViewProjectionMatrix() * entity->GetTransform().GetMatrix();
            const GPUConstants constants = GPUDevice::Get().GetConstantPool().Write(&mvpMatrix, sizeof(mvpMatrix));

            drawCall.arguments[0].argumentSet                = mArgumentSet;
            drawCall.arguments[0].constants[0].argumentIndex = 0;
            drawCall.arguments[0].constants[0].constants     = constants;
        }
    }

    /* Sort them. */
    context->drawList.Sort();

    /* Add the main pass. Done to a temporary render target with a fixed format,
     * since the output texture may not match the format that all PSOs have
     * been created with. */
    RenderGraphPass& pass = inGraph.AddPass("BasicScene", kRenderGraphPassType_Render);

    RenderTextureDesc colourTextureDesc(inGraph.GetTextureDesc(inTexture));
    colourTextureDesc.format = kColourFormat;

    RenderTextureDesc depthTextureDesc(inGraph.GetTextureDesc(inTexture));
    depthTextureDesc.format = kDepthFormat;

    RenderResourceHandle colourTexture = inGraph.CreateTexture(colourTextureDesc);
    RenderResourceHandle depthTexture  = inGraph.CreateTexture(depthTextureDesc);

    pass.SetColour(0, colourTexture, &colourTexture);
    pass.SetDepthStencil(depthTexture, kGPUResourceState_DepthStencilWrite);

    pass.ClearColour(0, this->clearColour);
    pass.ClearDepth(1.0f);

    pass.SetFunction([context] (const RenderGraph&      inGraph,
                                const RenderGraphPass&  inPass,
                                GPUGraphicsCommandList& inCmdList)
    {
        context->drawList.Draw(inCmdList);
    });

    /* Blit to the final output. */
    RenderGraphPass& blitPass = inGraph.AddPass("BasicBlit", kRenderGraphPassType_Transfer);

    blitPass.UseResource(colourTexture,
                         GPUSubresourceRange{0, 1, 0, 1},
                         kGPUResourceState_TransferRead,
                         nullptr);
    blitPass.UseResource(inTexture,
                         GPUSubresourceRange{0, 1, 0, 1},
                         kGPUResourceState_TransferWrite,
                         &outNewTexture);

    blitPass.SetFunction([colourTexture, inTexture] (const RenderGraph&     inGraph,
                                                     const RenderGraphPass& inPass,
                                                     GPUTransferContext&    inContext)
    {
        inContext.BlitTexture(inGraph.GetTexture(inTexture),
                              GPUSubresource{0, 0},
                              inGraph.GetTexture(colourTexture),
                              GPUSubresource{0, 0});
    });
}
