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

#include "GPU/GPUResource.h"

class GPUSwapchain;

struct GPUTextureDesc
{
    GPUResourceType         type;
    GPUResourceUsage        usage;
    GPUTextureFlags         flags;
    PixelFormat             format;

    uint32_t                width;
    uint32_t                height;
    uint32_t                depth;

    /**
     * Array size. Must be 1 for 3D textures. Must be a multiple of 6 for cube
     * compatible textures.
     */
    uint16_t                arraySize;

    /**
     * Number of mip levels. Specifying 0 here will give the texture a full mip
     * chain.
     */
    uint8_t                 numMipLevels;

};

class GPUTexture : public GPUResource
{
protected:
                            GPUTexture(GPUDevice&            inDevice,
                                       const GPUTextureDesc& inDesc);

                            GPUTexture(GPUSwapchain& inSwapchain);

                            ~GPUTexture() {}

public:
    GPUTextureFlags         GetFlags() const            { return mFlags; }
    bool                    IsCubeCompatible() const    { return mFlags & kGPUTexture_CubeCompatible; }

    PixelFormat             GetFormat() const           { return mFormat; }
    uint32_t                GetWidth() const            { return mWidth; }
    uint32_t                GetHeight() const           { return mHeight; }
    uint32_t                GetDepth() const            { return mDepth; }
    uint16_t                GetArraySize() const        { return mArraySize; }
    uint8_t                 GetNumMipLevels() const     { return mNumMipLevels; }

    uint32_t                GetMipWidth(const uint8_t inMip) const;
    uint32_t                GetMipHeight(const uint8_t inMip) const;
    uint32_t                GetMipDepth(const uint8_t inMip) const;

    /**
     * Gets the swapchain, if any, that the texture refers to. Swapchain
     * textures have special rules about when they safe to access - see
     * GPUSwapchain for details. It is also not allowed to create arbitrary
     * views of them: you can only use the view provided by the swapchain.
     */
    bool                    IsSwapchain() const         { return mSwapchain != nullptr; }
    GPUSwapchain*           GetSwapchain() const        { return mSwapchain; }

protected:
    const GPUTextureFlags   mFlags;
    const PixelFormat       mFormat;
    const uint32_t          mWidth;
    const uint32_t          mHeight;
    const uint32_t          mDepth;
    const uint16_t          mArraySize;
    uint8_t                 mNumMipLevels;

    GPUSwapchain*           mSwapchain;

};

using GPUTexturePtr = ReferencePtr<GPUTexture>;

inline uint32_t GPUTexture::GetMipWidth(const uint8_t inMip) const
{
    return std::max(mWidth >> inMip, 1u);
}

inline uint32_t GPUTexture::GetMipHeight(const uint8_t inMip) const
{
    return std::max(mHeight >> inMip, 1u);
}

inline uint32_t GPUTexture::GetMipDepth(const uint8_t inMip) const
{
    return std::max(mDepth >> inMip, 1u);
}
