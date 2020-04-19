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

class DeferredRenderPipelineWindow;
class GPUComputePipeline;
class TonemapPass;

struct DeferredRenderContext;

/**
 * Render pipeline implementation doing deferred lighting (render geometry and
 * material properties to a G-Buffer and then apply lighting in a separate
 * pass).
 */
class DeferredRenderPipeline final : public RenderPipeline
{
    CLASS();

public:
    static constexpr PixelFormat        kColourFormat = kPixelFormat_FloatR11G11B10;
    static constexpr PixelFormat        kDepthFormat  = kPixelFormat_Depth32;

    /**
     * G-Buffer layout:
     *
     *     | Format            | R            | G            | B            | A
     *  ---|-------------------|--------------|--------------|--------------|--------------
     *   0 | R8G8B8A8sRGB      | BaseColour.r | BaseColour.g | BaseColour.b | -
     *  ---|-------------------|--------------|--------------|--------------|--------------
     *   1 | R10G10B10A2       | Normal.x     | Normal.y     | Normal.z     | -
     *  ---|-------------------|--------------|--------------|--------------|--------------
     *   2 | R8G8B8A8          | Metallic     | Roughness    | Occlusion    | -
     *  ---|-------------------|--------------|--------------|--------------|--------------
     *   3 | R11G11B10         | Emissive.r   | Emissive.g   | Emissive.b   | -
     *
     * The normal buffer is an unsigned normalized format, therefore the normals
     * are scaled to fit into the [0, 1] range.
     *
     * Position is reconstructed from the depth buffer.
     *
     * Emissive is output directly to the main colour target, bound as 3, during
     * the G-Buffer pass.
     */
    static constexpr PixelFormat        kGBuffer0Format = kPixelFormat_R8G8B8A8sRGB;
    static constexpr PixelFormat        kGBuffer1Format = kPixelFormat_R10G10B10A2;
    static constexpr PixelFormat        kGBuffer2Format = kPixelFormat_R8G8B8A8;

public:
                                        DeferredRenderPipeline();

    void                                SetName(std::string inName) override;

    void                                Render(const RenderWorld&         inWorld,
                                               const RenderView&          inView,
                                               RenderGraph&               inGraph,
                                               const RenderResourceHandle inTexture,
                                               RenderResourceHandle&      outNewTexture) override;

protected:
                                        ~DeferredRenderPipeline();

private:
    void                                CreateShaders();

    void                                CreateResources(DeferredRenderContext* const inContext,
                                                        const RenderResourceHandle   inOutputTexture) const;

    void                                BuildDrawLists(DeferredRenderContext* const inContext) const;
    void                                PrepareLights(DeferredRenderContext* const inContext) const;
    void                                AddGBufferPasses(DeferredRenderContext* const inContext) const;
    void                                AddCullingPass(DeferredRenderContext* const inContext) const;
    void                                AddLightingPass(DeferredRenderContext* const inContext) const;

private:
    GPUShaderPtr                        mCullingShader;
    GPUShaderPtr                        mLightingShader;
    UPtr<GPUComputePipeline>            mCullingPipeline;
    UPtr<GPUComputePipeline>            mLightingPipeline;

    UPtr<TonemapPass>                   mTonemapPass;

    /** Debug visualisation flags. */
    bool                                mDrawEntityBoundingBoxes;
    bool                                mDrawLightVolumes;

    UPtr<DeferredRenderPipelineWindow>  mDebugWindow;

    friend class DeferredRenderPipelineWindow;
};
