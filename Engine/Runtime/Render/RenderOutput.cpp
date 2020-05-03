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

#include "Render/RenderOutput.h"

#include "GPU/GPUContext.h"

#include "Render/RenderGraph.h"
#include "Render/RenderLayer.h"
#include "Render/RenderManager.h"

RenderOutput::RenderOutput(const glm::uvec2& size) :
    mSize   (size)
{
}

RenderOutput::~RenderOutput()
{
    Assert(mLayers.empty());

    UnregisterOutput();
}

void RenderOutput::AddPasses(RenderGraph& graph,
                             OnlyCalledBy<RenderManager>)
{
    /* Import our output texture into the render graph. */
    RenderResourceHandle outputTexture =
        graph.ImportResource(GetTexture(),
                             GetFinalState(),
                             "Output",
                             [this] () { BeginRender(); },
                             [this] () { EndRender(); },
                             this);

    /* Each layer gets the previous layer's result handle as its target, so
     * that they get rendered on top of each other in order. */
    for (RenderLayer* layer : mLayers)
    {
        graph.SetCurrentLayer(layer);
        layer->AddPasses(graph, outputTexture, outputTexture);
    }
}

void RenderOutput::RegisterLayer(RenderLayer* const layer,
                                 OnlyCalledBy<RenderLayer>)
{
    /* List is sorted by order. */
    for (auto it = mLayers.begin(); it != mLayers.end(); ++it)
    {
        const RenderLayer* const other = *it;

        if (layer->GetLayerOrder() < other->GetLayerOrder())
        {
            mLayers.insert(it, layer);
            return;
        }
    }

    /* Insertion point not found, add at end. */
    mLayers.emplace_back(layer);
}

void RenderOutput::UnregisterLayer(RenderLayer* const layer,
                                   OnlyCalledBy<RenderLayer>)
{
    mLayers.remove(layer);
}

void RenderOutput::RegisterOutput()
{
    RenderManager::Get().RegisterOutput(this, {});
}

void RenderOutput::UnregisterOutput()
{
    RenderManager::Get().UnregisterOutput(this, {});
}

