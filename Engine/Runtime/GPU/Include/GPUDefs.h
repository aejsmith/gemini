/*
 * Copyright (C) 2018 Alex Smith
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

#include "Core/PixelFormat.h"
#include "Core/Utility.h"

enum GPUResourceUsage : uint32_t
{
    /** Resource will be bound as a read-only shader resource. */
    kGPUResourceUsage_ShaderRead    = (1 << 0),

    /** Resource will be bound as a writable shader resource. */
    kGPUResourceUsage_ShaderWrite   = (1 << 1),

    /** Resource will be bound as a render target. */
    kGPUResourceUsage_RenderTarget  = (1 << 2),

    /** Resource will be bound as a depth/stencil target. */
    kGPUResourceUsage_DepthStencil  = (1 << 3),
};

DEFINE_ENUM_BITWISE_OPS(GPUResourceUsage);

enum GPUResourceType
{
    kGPUResourceType_Buffer,
    kGPUResourceType_Texture1D,
    kGPUResourceType_Texture2D,
    kGPUResourceType_Texture3D,
};

enum GPUTextureFlags : uint32_t
{
    /**
     * Cube resource views can be created of the texture. When used, the type
     * must be kGPUResourceType_2D, and the array size must be specified as a
     * multiple of 6.
     */
    kGPUTexture_CubeCompatible = (1 << 0),
};

DEFINE_ENUM_BITWISE_OPS(GPUTextureFlags);

enum GPUResourceViewType
{
    /** Untyped view of a buffer. */
    kGPUResourceViewType_Buffer,

    /** Typed view of a buffer. */
    kGPUResourceViewType_TextureBuffer,

    /** 1D texture (array). Texture must be kGPUResourceType_1D. */
    kGPUResourceViewType_Texture1D,
    kGPUResourceViewType_Texture1DArray,

    /** 2D texture (array). Texture must be kGPUResourceType_2D. */
    kGPUResourceViewType_Texture2D,
    kGPUResourceViewType_Texture2DArray,

    /** Cube texture (array). Texture must have kGPUTexture_CubeCompatible. */
    kGPUResourceViewType_TextureCube,
    kGPUResourceViewType_TextureCubeArray,
    kGPUResourceViewType_Texture3D,
};
