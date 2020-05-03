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

#include "Render/RenderView.h"

class RenderGraph;
class RenderWorld;

/**
 * This class (and its derived pipeline-specific implementations) store
 * transient state used during a single execution of a RenderPipeline.
 */
class RenderContext
{
public:
                                RenderContext(RenderGraph&       graph,
                                              const RenderWorld& world,
                                              const RenderView&  view);

    RenderGraph&                GetGraph() const    { return mGraph; }
    const RenderWorld&          GetWorld() const    { return mWorld; }
    const RenderView&           GetView() const     { return mView; }

private:
    RenderGraph&                mGraph;
    const RenderWorld&          mWorld;

    /* Store a copy of the view since the context will usually outlive it. */
    const RenderView            mView;

};
