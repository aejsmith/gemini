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
#include "Engine/Profiler.h"

#include "GPU/GPUDefs.h"

#include "../../Shaders/ShaderDefs.h"

#define RENDER_PROFILER_NAME            "Render"
#define RENDER_PROFILER_COLOUR          0x00ff00
#define RENDER_PROFILER_SCOPE(timer)    PROFILER_SCOPE(RENDER_PROFILER_NAME, timer, RENDER_PROFILER_COLOUR)
#define RENDER_PROFILER_FUNC_SCOPE()    PROFILER_FUNC_SCOPE(RENDER_PROFILER_NAME, RENDER_PROFILER_COLOUR)

using ShaderDefineArray = std::vector<std::string>;

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
    /** Basic pass without any lighting etc. (BasicRenderPipeline) */
    kShaderPassType_Basic,

    /** Deferred opaque G-Buffer pass (DeferredRenderPipeline). */
    kShaderPassType_DeferredOpaque,

    /** Deferred unlit pass (DeferredRenderPipeline). */
    kShaderPassType_DeferredUnlit,

    /** Shadow map rendering. */
    kShaderPassType_ShadowMap,

    kShaderPassTypeCount
};

/**
 * Flags controlling the behaviour of a shader pass. These can be set by shader
 * techniques (possibly conditionally based on material feature flags) to
 * adjust the behaviour of the pass type for entities using the
 * technique/material. For example, these might adjust some of the pipeline
 * state.
 *
 * Note that for the serialisation system, these values need to be unique
 * across all pass types.
 */
enum ENUM() ShaderPassFlags
{
    kShaderPassFlags_None = 0,

    /**
     * kShaderPassType_DeferredOpaque
     */

    /** Material is emissive (causes emissive target output to be enabled). */
    kShaderPassFlags_DeferredOpaque_Emissive = 1 << 0,
};

DEFINE_ENUM_BITWISE_OPS(ShaderPassFlags);

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

    kLightTypeCount
};
