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

#include "Engine/DebugManager.h"

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
}

BasicRenderPipeline::~BasicRenderPipeline()
{
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
        }
    }

    /* Sort them. */
    context->drawList.Sort();

    /* Add the main pass. Done to a temporary render target with a fixed format,
     * since the output texture may not match the format that all PSOs have
     * been created with. */
    RenderGraphPass& mainPass = inGraph.AddPass("BasicMain", kRenderGraphPassType_Render);

    RenderTextureDesc colourTextureDesc(inGraph.GetTextureDesc(inTexture));
    colourTextureDesc.format = kColourFormat;

    RenderTextureDesc depthTextureDesc(inGraph.GetTextureDesc(inTexture));
    depthTextureDesc.format = kDepthFormat;

    RenderResourceHandle colourTexture = inGraph.CreateTexture(colourTextureDesc);
    RenderResourceHandle depthTexture  = inGraph.CreateTexture(depthTextureDesc);

    mainPass.SetColour(0, colourTexture, &colourTexture);
    mainPass.SetDepthStencil(depthTexture, kGPUResourceState_DepthStencilWrite);

    mainPass.ClearColour(0, this->clearColour);
    mainPass.ClearDepth(1.0f);

    context->drawList.Draw(mainPass);

    /* Blit to the final output. */
    inGraph.AddBlitPass("BasicBlit",
                        inTexture,
                        GPUSubresource{0, 0},
                        colourTexture,
                        GPUSubresource{0, 0},
                        &outNewTexture);

    /* Render debug primitives for the view. */
    DebugManager::Get().RenderPrimitives(inView,
                                         inGraph,
                                         outNewTexture,
                                         outNewTexture);
}
