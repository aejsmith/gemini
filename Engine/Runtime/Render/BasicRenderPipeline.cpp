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

void BasicRenderPipeline::Render(const RenderWorld&         world,
                                 const RenderView&          view,
                                 RenderGraph&               graph,
                                 const RenderResourceHandle texture,
                                 RenderResourceHandle&      outNewTexture)
{
    BasicRenderContext* const context = graph.NewTransient<BasicRenderContext>(graph, world, view);

    /* Get the visible entities. */
    world.Cull(view, context->cullResults);

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
    RenderGraphPass& mainPass = graph.AddPass("BasicMain", kRenderGraphPassType_Render);

    RenderTextureDesc colourTextureDesc(graph.GetTextureDesc(texture));
    colourTextureDesc.format = kColourFormat;

    RenderTextureDesc depthTextureDesc(graph.GetTextureDesc(texture));
    depthTextureDesc.format = kDepthFormat;

    RenderResourceHandle colourTexture = graph.CreateTexture(colourTextureDesc);
    RenderResourceHandle depthTexture  = graph.CreateTexture(depthTextureDesc);

    mainPass.SetColour(0, colourTexture, &colourTexture);
    mainPass.SetDepthStencil(depthTexture, kGPUResourceState_DepthStencilWrite);

    mainPass.ClearColour(0, this->clearColour);
    mainPass.ClearDepth(1.0f);

    context->drawList.Draw(mainPass);

    /* Blit to the final output. */
    graph.AddBlitPass("BasicBlit",
                      texture,
                      GPUSubresource{0, 0},
                      colourTexture,
                      GPUSubresource{0, 0},
                      &outNewTexture);

    /* Render debug primitives for the view. */
    DebugManager::Get().RenderPrimitives(view,
                                         graph,
                                         outNewTexture,
                                         outNewTexture);
}
