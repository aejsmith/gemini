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

#include "Render/EntityDrawList.h"
#include "Render/RenderContext.h"
#include "Render/RenderWorld.h"

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
    for (const RenderEntity* entity : context->cullResults.entities)
    {
        // TODO: Move into the entity.
    }

    /* Sort them. */
    context->drawList.Sort();

    /* Add the main pass. */
    RenderGraphPass& pass = inGraph.AddPass("Scene", kRenderGraphPassType_Render);

    pass.SetColour(0, inTexture, &outNewTexture);
    pass.ClearColour(0, this->clearColour);

    pass.SetFunction([context] (const RenderGraph&      inGraph,
                                const RenderGraphPass&  inPass,
                                GPUGraphicsCommandList& inCmdList)
    {
        context->drawList.Draw(inCmdList);

        // debug renderer bounding boxes
    });
}
