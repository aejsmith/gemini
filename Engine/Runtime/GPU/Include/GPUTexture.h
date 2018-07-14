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

struct GPUTextureDesc
{
    GPUResourceType         type;
    GPUResourceUsage        usage;
    GPUTextureFlags         flags;
    uint32_t                width;
    uint32_t                height;
    uint32_t                depth;
    uint16_t                arraySize;
    uint16_t                numMipLevels;
    PixelFormat             format;
};

class GPUTexture : public GPUResource
{
protected:
                            GPUTexture(GPUDevice&            inDevice,
                                       const GPUTextureDesc& inDesc);

                            ~GPUTexture() {}

public:
    GPUTextureFlags         GetFlags() const        { return mFlags; }
    uint32_t                GetWidth() const        { return mWidth; }
    uint32_t                GetHeight() const       { return mHeight; }
    uint32_t                GetDepth() const        { return mDepth; }
    uint16_t                GetArraySize() const    { return mArraySize; }
    uint16_t                GetNumMipLevels() const { return mNumMipLevels; }
    PixelFormat             GetFormat() const       { return mFormat; }

protected:
    const GPUTextureFlags   mFlags;
    const uint32_t          mWidth;
    const uint32_t          mHeight;
    const uint32_t          mDepth;
    const uint16_t          mArraySize;
    const uint16_t          mNumMipLevels;
    const PixelFormat       mFormat;

};

using GPUTexturePtr = ReferencePtr<GPUTexture>;
