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

#include "GPU/GPUTexture.h"

#include "GPU/GPUSwapchain.h"

#include "Engine/Window.h"

GPUTexture::GPUTexture(GPUDevice&            device,
                       const GPUTextureDesc& desc) :
    GPUResource     (device, desc.type, desc.usage),
    mFlags          (desc.flags),
    mFormat         (desc.format),
    mWidth          (desc.width),
    mHeight         (desc.height),
    mDepth          (desc.depth),
    mArraySize      (desc.arraySize),
    mNumMipLevels   (desc.numMipLevels),
    mSwapchain      (nullptr)
{
    Assert(GetType() != kGPUResourceType_Buffer);
    Assert(GetType() >= kGPUResourceType_Texture2D || GetWidth() == 1);
    Assert(GetType() == kGPUResourceType_Texture3D || GetDepth() == 1);
    Assert(!IsSwapchain());
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

GPUTexture::GPUTexture(GPUSwapchain& swapchain) :
    GPUResource     (swapchain.GetDevice(),
                     kGPUResourceType_Texture2D,
                     kGPUResourceUsage_RenderTarget),
    mFlags          (kGPUTexture_None),
    mFormat         (swapchain.GetFormat()),
    mWidth          (swapchain.GetWindow().GetSize().x),
    mHeight         (swapchain.GetWindow().GetSize().y),
    mDepth          (1),
    mArraySize      (1),
    mNumMipLevels   (1),
    mSwapchain      (&swapchain)
{
}

GPUSubresourceRange GPUTexture::GetSubresourceRange() const
{
    return { 0, mNumMipLevels, 0, mArraySize };
}
