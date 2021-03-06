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

#include "GPU/GPUStagingResource.h"

#include "GPU/GPUDevice.h"

GPUStagingResource::~GPUStagingResource()
{
    if (IsAllocated())
    {
        GetDevice().GetStagingPool().Free(mHandle);
    }
}

void GPUStagingResource::Allocate(const GPUStagingAccess access,
                                  const uint32_t         size)
{
    if (IsAllocated())
    {
        GetDevice().GetStagingPool().Free(mHandle);
    }

    mHandle    = GetDevice().GetStagingPool().Allocate(access, size, mMapping);
    mAccess    = access;
    mFinalised = false;
}

GPUStagingTexture::GPUStagingTexture() :
    GPUStagingResource  ()
{
}

GPUStagingTexture::~GPUStagingTexture()
{
}

void GPUStagingTexture::Initialise(const GPUStagingAccess access,
                                   const GPUTextureDesc&  desc)
{
    mDesc = desc;

    const uint32_t subresourceCount = GetNumMipLevels() * GetArraySize();
    mSubresourceOffsets.resize(subresourceCount);

    const uint32_t bytesPerPixel = PixelFormatInfo::BytesPerPixel(GetFormat());

    uint32_t bufferSize       = 0;
    uint32_t subresourceIndex = 0;
    for (uint32_t layer = 0; layer < GetArraySize(); layer++)
    {
        uint32_t width  = GetWidth();
        uint32_t height = GetHeight();
        uint32_t depth  = GetDepth();

        for (uint32_t mipLevel = 0; mipLevel < GetNumMipLevels(); mipLevel++)
        {
            mSubresourceOffsets[subresourceIndex] = bufferSize;

            bufferSize += bytesPerPixel * width * height * depth;
            subresourceIndex++;

            width  = std::max(width  >> 1, 1u);
            height = std::max(height >> 1, 1u);
            depth  = std::max(depth  >> 1, 1u);
        }
    }

    Allocate(access, bufferSize);
}

void* GPUStagingTexture::MapWrite(const GPUSubresource subresource)
{
    Assert(!IsFinalised());

    const uint32_t offset = GetSubresourceOffset(subresource);
    return reinterpret_cast<uint8_t*>(mMapping) + offset;
}
