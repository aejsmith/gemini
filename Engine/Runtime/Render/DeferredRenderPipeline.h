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
#include "GPU/GPUPipeline.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUState.h"

#include "Render/RenderPipeline.h"

class DeferredRenderPipelineWindow;
class FXAAPass;
class GPUBuffer;
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

    /**
     * In the deferred render pipeline, shadows are rendered by rendering a
     * shadow map for each shadow casting light, and then projecting this into
     * a screen-space shadow mask texture. The mask is an array texture which
     * has a layer per shadow casting light. Each layer has a single R8 UNorm
     * channel which encodes the shadow attenuation factor at each pixel.
     */
    static constexpr PixelFormat        kShadowMaskFormat = kPixelFormat_R8;

public:
                                        DeferredRenderPipeline();

    void                                SetName(std::string name) override;

    void                                Render(const RenderWorld&    world,
                                               const RenderView&     view,
                                               RenderGraph&          graph,
                                               RenderResourceHandle& ioDestTexture) override;

    /** Whether FXAA is enabled. */
    VPROPERTY(bool, enableFXAA);
    bool                                GetEnableFXAA() const { return mFXAAPass != nullptr; }
    void                                SetEnableFXAA(const bool enable);

public:
    /**
     * Maximum number of shadow casting lights per-frame. The world as a whole
     * can have any number of them, this just limits the number that can be in
     * view at once. If there are too many, shadows will not be rendered for
     * some of them.
     *
     * This determines the number of layers in the shadow mask, therefore
     * increasing it increases VRAM usage.
     */
    PROPERTY() uint16_t                 maxShadowLights;

    /**
     * Constant bias value applied to values sampled from the shadow map, used
     * to eliminate shadow acne. TODO: More biasing options (slope scaled, or
     * normal offset), make this per-light?
     */
    PROPERTY() float                    shadowBiasConstant;

protected:
                                        ~DeferredRenderPipeline();

private:
    void                                CreateShaders();
    void                                CreatePersistentResources();

    void                                CreateResources(DeferredRenderContext* const context,
                                                        const RenderResourceHandle   destTexture) const;

    void                                BuildDrawLists(DeferredRenderContext* const context) const;
    void                                PrepareLights(DeferredRenderContext* const context) const;
    void                                AddGBufferPasses(DeferredRenderContext* const context) const;
    void                                AddShadowPasses(DeferredRenderContext* const context) const;
    void                                AddCullingPass(DeferredRenderContext* const context) const;
    void                                AddLightingPass(DeferredRenderContext* const context) const;
    void                                AddUnlitPass(DeferredRenderContext* const context) const;
    void                                AddPostPasses(DeferredRenderContext* const context,
                                                      RenderResourceHandle&        ioDestTexture) const;

    void                                AddCullingDebugPass(DeferredRenderContext* const context,
                                                            RenderResourceHandle&        ioDestTexture) const;

    void                                DrawLightVolume(DeferredRenderContext* const context,
                                                        const RenderLight* const     light,
                                                        GPUGraphicsCommandList&      cmdList) const;

private:
    GPUShaderPtr                        mCullingShader;
    GPUShaderPtr                        mLightingShader;
    GPUShaderPtr                        mShadowMaskVertexShaders[kLightTypeCount];
    GPUShaderPtr                        mShadowMaskPixelShaders[kLightTypeCount];

    UPtr<GPUComputePipeline>            mCullingPipeline;
    UPtr<GPUComputePipeline>            mLightingPipeline;
    GPUPipelineRef                      mShadowMaskPipelines[kLightTypeCount];

    UPtr<GPUBuffer>                     mConeVertexBuffer;
    UPtr<GPUBuffer>                     mConeIndexBuffer;

    GPUSamplerRef                       mShadowMapSampler;

    UPtr<TonemapPass>                   mTonemapPass;
    UPtr<FXAAPass>                      mFXAAPass;

    /**
     * Debug-only stuff.
     */

    bool                                mDebugEntityBoundingBoxes;
    bool                                mDebugLightVolumes;
    bool                                mDebugLightCulling;
    int                                 mDebugLightCullingMaximum;

    UPtr<DeferredRenderPipelineWindow>  mDebugWindow;

    GPUShaderPtr                        mCullingDebugVertexShader;
    GPUShaderPtr                        mCullingDebugPixelShader;
    GPUArgumentSetLayoutRef             mCullingDebugArgumentSetLayout;

    friend class DeferredRenderPipelineWindow;
};
