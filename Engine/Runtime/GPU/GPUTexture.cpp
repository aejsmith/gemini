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

#include "GPU/GPUTexture.h"

GPUTexture::GPUTexture(GPUDevice&            inDevice,
                       const GPUTextureDesc& inDesc) :
    GPUResource     (inDevice,
                     inDesc.type,
                     inDesc.usage),
    mFlags          (inDesc.flags),
    mFormat         (inDesc.format),
    mWidth          (inDesc.width),
    mHeight         (inDesc.height),
    mDepth          (inDesc.depth),
    mArraySize      (inDesc.arraySize),
    mNumMipLevels   (inDesc.numMipLevels)
{
    Assert(GetType() != kGPUResourceType_Buffer);
    Assert(GetType() >= kGPUResourceType_Texture2D || GetWidth() == 1);
    Assert(GetType() == kGPUResourceType_Texture3D || GetDepth() == 1);
    Assert(!IsCubeCompatible() || GetType() == kGPUResourceType_Texture2D);
    Assert(!IsCubeCompatible() || (GetArraySize() % 6) == 0);

    /* Calculate the maximum mip level count. */
    uint32_t width  = GetWidth();
    uint32_t height = (GetType() >= kGPUResourceType_Texture2D) ? GetHeight() : 1;
    uint32_t depth  = (GetType() == kGPUResourceType_Texture3D) ? GetDepth() : 1;

    uint8_t maxMips = 1;

    while (width > 1 || height > 1 || depth > 1)
    {
        if (width > 1)  width  >>= 1;
        if (height > 1) height >>= 1;
        if (depth > 1)  depth  >>= 1;

        maxMips++;
    }

    Assert(GetNumMipLevels() <= maxMips);

    if (GetNumMipLevels() == 0)
    {
        mNumMipLevels = maxMips;
    }
}
