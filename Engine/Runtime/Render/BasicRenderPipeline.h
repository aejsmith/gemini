/*
 * Copyright (C) 2018-2020 Alex Smith
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

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"

#include "Render/RenderPipeline.h"

/**
 * Extremely basic render pipeline implementation which renders the objects in
 * the world with no lighting etc.
 */
class BasicRenderPipeline final : public RenderPipeline
{
    CLASS();

public:
    static constexpr PixelFormat kColourFormat = kPixelFormat_R8G8B8A8;
    static constexpr PixelFormat kDepthFormat  = kPixelFormat_Depth32;

public:
                            BasicRenderPipeline();

    /** Colour to clear the background to. */
    PROPERTY()
    glm::vec4               clearColour;

    void                    Render(const RenderWorld&         world,
                                   const RenderView&          view,
                                   RenderGraph&               graph,
                                   const RenderResourceHandle texture,
                                   RenderResourceHandle&      outNewTexture) override;

protected:
                            ~BasicRenderPipeline();

};
