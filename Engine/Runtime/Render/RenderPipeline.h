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

#include "Engine/Object.h"

#include "Render/RenderGraph.h"
#include "Render/RenderView.h"

/**
 * This is the base class for a render pipeline, which implements the process
 * for rendering a world. A camera contains a render pipeline, which, when
 * enabled, will call into from the render graph to add all passes needed to
 * the graph to render the world.
 *
 * The pipeline is a persistent object, and is serialised with the camera that
 * owns it, so that persistent configuration of the rendering process can be
 * stored on it.
 *
 * Transient per-frame rendering state is stored in the RenderContext (or a
 * pipeline-specific derived implementation of that).
 */
class RenderPipeline : public Object
{
    CLASS();

public:
    /**
     * Add render graph passes to render everything visible from the given view
     * into the texture. The supplied handle is the texture that the view
     * should be rendered to. A new resource handle should be returned
     * referring to the rendered output.
     */
    virtual void            Render(const RenderView&          inView,
                                   RenderGraph&               inGraph,
                                   const RenderResourceHandle inTexture,
                                   RenderResourceHandle&      outNewTexture) = 0;

protected:
                            RenderPipeline();
                            ~RenderPipeline();

};
