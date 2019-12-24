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

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"

#include "Render/RenderPipeline.h"

#include <memory>

class DeferredRenderPipelineWindow;

/**
 * Render pipeline implementation doing deferred lighting (render geometry and
 * material properties to a G-Buffer and then apply lighting in a separate
 * pass).
 */
class DeferredRenderPipeline final : public RenderPipeline
{
    CLASS();

public:
    static constexpr PixelFormat kColourFormat = kPixelFormat_R8G8B8A8;
    static constexpr PixelFormat kDepthFormat  = kPixelFormat_Depth32;

public:
                            DeferredRenderPipeline();

    /** Colour to clear the background to. */
    PROPERTY()
    glm::vec4               clearColour;

    void                    SetName(std::string inName) override;

    void                    Render(const RenderWorld&         inWorld,
                                   const RenderView&          inView,
                                   RenderGraph&               inGraph,
                                   const RenderResourceHandle inTexture,
                                   RenderResourceHandle&      outNewTexture) override;

protected:
                            ~DeferredRenderPipeline();

private:
    /** Debug visualisation flags. */
    bool                    mDrawEntityBoundingBoxes;
    bool                    mDrawLightVolumes;

    UPtr<DeferredRenderPipelineWindow>
                            mDebugWindow;

    friend class DeferredRenderPipelineWindow;
};
