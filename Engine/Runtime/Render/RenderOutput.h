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

#pragma once

#include "Render/RenderDefs.h"

#include <list>

class GPUTexture;
class RenderGraph;
class RenderLayer;
class RenderManager;

/**
 * This class is the base for a final output of a render graph (a window, or a
 * texture that can then be used when rendering another output).
 *
 * An output is made up of layers, e.g. a world layer which renders the game
 * world, a layer for game UI, and an ImGUI layer. Registered layers on an
 * output are called based on their specified order to add their passes to the
 * render graph, with the effect that they will be composited in that order to
 * produce the final output (each layer receives a resource handle referring to
 * the result of the layer below).
 */
class RenderOutput
{
protected:
                                RenderOutput();
                                ~RenderOutput();

public:
    void                        AddPasses(RenderGraph& inGraph,
                                          OnlyCalledBy<RenderManager>);

    void                        RegisterLayer(RenderLayer* const inLayer,
                                              OnlyCalledBy<RenderLayer>);
    void                        UnregisterLayer(RenderLayer* const inLayer,
                                                OnlyCalledBy<RenderLayer>);

    /** Get the texture for this output. */
    virtual GPUTexture*         GetTexture() const = 0;

protected:
    void                        RegisterOutput();
    void                        UnregisterOutput();

    /**
     * Get the required final resource state of this output. It is expected
     * that the resource is in this state prior to rendering, and will be left
     * in this state after rendering.
     */
    virtual GPUResourceState    GetFinalState() const = 0;

    /**
     * Called around rendering to the output. This is for rendering to a
     * swapchain, since we must call {Begin,End}Present() around doing so.
     */
    virtual void                BeginRender()   {}
    virtual void                EndRender()     {}

private:
    std::list<RenderLayer*>     mLayers;

};
