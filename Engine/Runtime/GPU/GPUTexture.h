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

inline bool operator==(const GPUTextureDesc& a, const GPUTextureDesc& b)
{
    return a.type         == b.type         &&
           a.usage        == b.usage        &&
           a.flags        == b.flags        &&
           a.format       == b.format       &&
           a.width        == b.width        &&
           a.height       == b.height       &&
           a.depth        == b.depth        &&
           a.arraySize    == b.arraySize    &&
           a.numMipLevels == b.numMipLevels;
}

class GPUTexture : public GPUResource
{
protected:
                            GPUTexture(GPUDevice&            device,
                                       const GPUTextureDesc& desc);

                            GPUTexture(GPUSwapchain& swapchain);

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

    uint32_t                GetMipWidth(const uint8_t map) const;
    uint32_t                GetMipHeight(const uint8_t map) const;
    uint32_t                GetMipDepth(const uint8_t map) const;

    GPUSubresourceRange     GetSubresourceRange() const override;

    /**
     * It is valid to specify 0 counts in a GPUSubresourceRange to specify the
     * whole image. This is for internal use to replace this with the exact
     * range.
     */
    GPUSubresourceRange     GetExactSubresourceRange(const GPUSubresourceRange& range) const;

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
    bool                    SizeMatches(const T& other) const;

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

inline uint32_t GPUTexture::GetMipWidth(const uint8_t mip) const
{
    return std::max(mWidth >> mip, 1u);
}

inline uint32_t GPUTexture::GetMipHeight(const uint8_t mip) const
{
    return std::max(mHeight >> mip, 1u);
}

inline uint32_t GPUTexture::GetMipDepth(const uint8_t mip) const
{
    return std::max(mDepth >> mip, 1u);
}

inline GPUSubresourceRange GPUTexture::GetExactSubresourceRange(const GPUSubresourceRange& range) const
{
    if (range.layerCount != 0 || range.mipCount != 0)
    {
        return range;
    }
    else
    {
        return { 0, GetNumMipLevels(), 0, GetArraySize() };
    }
}

template <typename T>
inline bool GPUTexture::SizeMatches(const T& other) const
{
    return mWidth        == other.GetWidth() &&
           mHeight       == other.GetHeight() &&
           mDepth        == other.GetDepth() &&
           mArraySize    == other.GetArraySize() &&
           mNumMipLevels == other.GetNumMipLevels();
}
