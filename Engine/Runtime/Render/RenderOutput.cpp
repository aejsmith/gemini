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

#include "Render/RenderLayer.h"
#include "Render/RenderManager.h"

RenderOutput::RenderOutput()
{
}

RenderOutput::~RenderOutput()
{
    Assert(mLayers.empty());

    UnregisterOutput();
}

void RenderOutput::Render(OnlyCalledBy<RenderManager>)
{
    // TODO: Only want to call this around final access from render graph to
    // avoid acquiring swapchain image earlier than it needs to be.
    BeginRender();

    // TODO: Will be handled by render graph.
    GPUGraphicsContext::Get().ResourceBarrier(GetTexture(),
                                              GetFinalState(),
                                              kGPUResourceState_RenderTarget);

    for (RenderLayer* layer : mLayers)
    {
        layer->Render();
    }

    // TODO: Same as above.
    GPUGraphicsContext::Get().ResourceBarrier(GetTexture(),
                                              kGPUResourceState_RenderTarget,
                                              GetFinalState());

    EndRender();
}

void RenderOutput::RegisterLayer(RenderLayer* const inLayer,
                                 OnlyCalledBy<RenderLayer>)
{
    /* List is sorted by order. */
    for (auto it = mLayers.begin(); it != mLayers.end(); ++it)
    {
        const RenderLayer* const other = *it;

        if (inLayer->GetLayerOrder() < other->GetLayerOrder())
        {
            mLayers.insert(it, inLayer);
            return;
        }
    }

    /* Insertion point not found, add at end. */
    mLayers.emplace_back(inLayer);
}

void RenderOutput::UnregisterLayer(RenderLayer* const inLayer,
                                   OnlyCalledBy<RenderLayer>)
{
    mLayers.remove(inLayer);
}

void RenderOutput::RegisterOutput()
{
    RenderManager::Get().RegisterOutput(this, {});
}

void RenderOutput::UnregisterOutput()
{
    RenderManager::Get().UnregisterOutput(this, {});
}

