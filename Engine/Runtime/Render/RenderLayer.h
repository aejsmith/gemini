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

class RenderGraph;
class RenderOutput;
struct RenderResourceHandle;

/**
 * Layer on a RenderOutput. See RenderOutput for more details.
 */
class RenderLayer
{
public:
    /** Rendering order. Lower numbers are rendered first. */
    enum Order : uint8_t
    {
        /** Game world. */
        kOrder_World    = 10,

        /** Game UI. */
        kOrder_UI       = 20,

        /** ImGUI overlay. */
        kOrder_ImGUI    = 30,
    };

protected:
                            RenderLayer(const uint8_t inOrder);
                            ~RenderLayer();

public:
    uint8_t                 GetLayerOrder() const   { return mOrder; }

    RenderOutput*           GetLayerOutput() const  { return mOutput; }
    void                    SetLayerOutput(RenderOutput* const inOutput);

    /**
     * (De)activate the layer. The layer will only be rendered while active. A
     * valid output must be set.
     */
    void                    ActivateLayer();
    void                    DeactivateLayer();
    bool                    IsLayerActive() const   { return mActive; }

protected:
    /**
     * Add render passes to the render graph for this layer. The supplied
     * handle is the texture that the layer output should be written to. If the
     * layer has anything to render, a new resource handle should be returned.
     */
    virtual void            AddPasses(RenderGraph&               inGraph,
                                      const RenderResourceHandle inTexture,
                                      RenderResourceHandle&      outNewTexture) = 0;

private:
    const uint8_t           mOrder;
    RenderOutput*           mOutput;
    bool                    mActive;

    friend class RenderOutput;
};
