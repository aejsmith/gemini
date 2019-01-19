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

#include "GPU/GPUResource.h"

class GPUSwapchain;

struct GPUTextureDesc
{
    GPUResourceType         type;
    GPUResourceUsage        usage           = kGPUResourceUsage_Standard;
    GPUTextureFlags         flags           = kGPUTexture_None;
    PixelFormat             format;

    uint32_t                width           = 1;
    uint32_t                height          = 1;
    uint32_t                depth           = 1;

    /**
     * Array size. Must be 1 for 3D textures. Must be a multiple of 6 for cube
     * compatible textures.
     */
    uint16_t                arraySize       = 1;

    /**
     * Number of mip levels. Specifying 0 here will give the texture a full mip
     * chain.
     */
    uint8_t                 numMipLevels    = 1;

};

class GPUTexture : public GPUResource
{
protected:
                            GPUTexture(GPUDevice&            inDevice,
                                       const GPUTextureDesc& inDesc);

                            GPUTexture(GPUSwapchain& inSwapchain);

public:
                            ~GPUTexture() {}

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
     * It is valid to specify 0 counts in a GPUSubresourceRange to specify the
     * whole image. This is for internal use to replace this with the exact
     * range.
     */
    GPUSubresourceRange     GetExactSubresourceRange(const GPUSubresourceRange& inRange) const;

    /**
     * Gets the swapchain, if any, that the texture refers to. Swapchain
     * textures have special rules about when they safe to access - see
     * GPUSwapchain for details. It is also not allowed to create arbitrary
     * views of them: you can only use the view provided by the swapchain.
     */
    bool                    IsSwapchain() const         { return mSwapchain != nullptr; }
    GPUSwapchain*           GetSwapchain() const        { return mSwapchain; }

    /**
     * Determine whether the size (dimensions, subresources) of this texture
     * matches another. Templated to work for both another GPUTexture and a
     * GPUStagingTexture.
     */
    template <typename T>
    bool                    SizeMatches(const T& inOther) const;

private:
    const GPUTextureFlags   mFlags;
    const PixelFormat       mFormat;
    const uint32_t          mWidth;
    const uint32_t          mHeight;
    const uint32_t          mDepth;
    const uint16_t          mArraySize;
    uint8_t                 mNumMipLevels;

    GPUSwapchain*           mSwapchain;

};

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

inline GPUSubresourceRange GPUTexture::GetExactSubresourceRange(const GPUSubresourceRange& inRange) const
{
    if (inRange.layerCount != 0 || inRange.mipCount != 0)
    {
        return inRange;
    }
    else
    {
        return { 0, GetNumMipLevels(), 0, GetArraySize() };
    }
}

template <typename T>
inline bool GPUTexture::SizeMatches(const T& inOther) const
{
    return mWidth        == inOther.GetWidth() &&
           mHeight       == inOther.GetHeight() &&
           mDepth        == inOther.GetDepth() &&
           mArraySize    == inOther.GetArraySize() &&
           mNumMipLevels == inOther.GetNumMipLevels();
}
