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

#include "Engine/Object.h"

#include "Render/RenderGraph.h"

class RenderLight;
class RenderView;
class RenderWorld;

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
    static constexpr PixelFormat    kShadowMapFormat   = kPixelFormat_Depth32;
    static constexpr uint8_t        kMaxShadowCascades = 4;

public:
    /**
     * Add render graph passes to render everything visible from the given view
     * into the texture. The supplied handle is the texture that the view
     * should be rendered to, which should be overwritten with a handle to a
     * new version of the resource for the rendered output.
     */
    virtual void                    Render(const RenderWorld&    world,
                                           const RenderView&     view,
                                           RenderGraph&          graph,
                                           RenderResourceHandle& ioDestTexture) = 0;

    /** Get/set the name of the pipeline (used for debug purposes). */
    const std::string&              GetName() const { return mName; }
    virtual void                    SetName(std::string name)
                                        { mName = std::move(name); }

    /** For directional light CSMs, the number of cascades to use (1 to 4). */
    VPROPERTY(uint8_t, directionalShadowCascades);
    uint8_t                         GetDirectionalShadowCascades() const { return mDirectionalShadowCascades; }
    void                            SetDirectionalShadowCascades(const uint8_t cascades);

    /**
     * For directional light CSMs, the factor which determines distribution of
     * cascade splits. 0 will give exactly linear distribution of splits, 1 will
     * give exactly exponential distribution. Values in between interpolate
     * between the two.
     */
    VPROPERTY(float, directionalShadowSplitFactor);
    float                           GetDirectionalShadowSplitFactor() const { return mDirectionalShadowSplitFactor; }
    void                            SetDirectionalShadowSplitFactor(const float factor);

public:
    /**
     * Maximum visible distance for directional shadows. It is recommended to
     * keep this fairly short both for performance reasons (cost of rendering
     * objects to the shadow maps), but also to get better distribution of the
     * distance across shadow cascades.
     */
    PROPERTY() float                directionalShadowMaxDistance;

    /** Resolution to use for shadow maps. */
    PROPERTY() uint16_t             shadowMapResolution;

protected:
                                    RenderPipeline();
                                    ~RenderPipeline();

    RenderResourceHandle            CreateShadowMap(RenderGraph&    graph,
                                                    const LightType lightType) const;

    /**
     * Calculate all the views to render into the shadow map. Output array is
     * indexed by the array layer of the shadow map that each view should be
     * rendered to.
     */
    void                            CreateShadowViews(const RenderLight&       light,
                                                      const RenderView&        cameraView,
                                                      std::vector<RenderView>& outViews,
                                                      float* const             outSplitDepths) const;

private:
    std::string                     mName;

    uint8_t                         mDirectionalShadowCascades;
    float                           mDirectionalShadowSplitFactor;

};
