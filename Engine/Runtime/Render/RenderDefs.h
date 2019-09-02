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

#include "GPU/GPUDefs.h"

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

/**
 * Argument set indices used by the main renderer.
 */
enum ArgumentSet
{
    /** View arguments. */
    kArgumentSet_View,

    /** Material arguments. */
    kArgumentSet_Material,

    /** Entity arguments. */
    kArgumentSet_Entity,
};
