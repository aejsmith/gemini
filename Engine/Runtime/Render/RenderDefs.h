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

#include "GPU/GPUDefs.h"

#include "../../Shaders/ShaderDefs.h"

class GPUTransferContext;

/**
 * A render pipeline will perform a number of render passes, each of which will
 * need to render a subset of visible entities. The passes that an entity will
 * be rendered in are defined by the shader technique that the entity is using.
 * A technique defines a pass for each type of render pass that it can support
 * being rendered in. For each render pass performed by a render pipeline, if
 * a visible entity's technique has a pass of that type, it will be rendered in
 * that pass.
 *
 * A pass type defines defaults for pipeline state (blend, depth/stencil,
 * rasterizer, render target), and possibly defines some shader variant flags.
 * Some pipeline state can be overridden by the technique.
 */
enum ENUM() ShaderPassType
{
    /**
     * Basic pass without any lighting etc. (BasicRenderPipeline)
     */
    kShaderPassType_Basic,

    /**
     * Deferred G-Buffer opaque pass (DeferredRenderPipeline).
     */
    kShaderPassType_DeferredOpaque,

    kShaderPassTypeCount,
};

/** Types of light. */
enum ENUM() LightType
{
    /**
     * Light source that is infinitely far away and emits light in a uniform
     * direction, along the parent entity's local negative Z axis.
     */
    kLightType_Directional,

    /**
     * Light source that emits light in all directions from their position in
     * space.
     */
    kLightType_Point,

    /**
     * Light source that emits light in a cone centered around the parent
     * entity's local negative Z axis.
     */
    kLightType_Spot,
};

/** Scoped debug marker class, does nothing on debug builds. */
class ScopedDebugMarker
{
public:
                            ScopedDebugMarker(GPUTransferContext& context,
                                              const char* const   label);

                            ScopedDebugMarker(GPUTransferContext& context,
                                              const std::string&  label);

                            ~ScopedDebugMarker();

private:
    #if GEMINI_BUILD_DEBUG
    GPUTransferContext&     mContext;
    #endif
};

#if !GEMINI_BUILD_DEBUG

inline ScopedDebugMarker::ScopedDebugMarker(GPUTransferContext& context,
                                            const char* const   label)
{
}

inline ScopedDebugMarker::ScopedDebugMarker(GPUTransferContext& context,
                                            const std::string&  label)
{
}

inline ScopedDebugMarker::~ScopedDebugMarker()
{
}

#endif

#define SCOPED_DEBUG_MARKER(context, label) \
    ScopedDebugMarker label_ ## __LINE__ (context, label)
