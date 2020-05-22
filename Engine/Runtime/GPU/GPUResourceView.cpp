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

#include "GPU/GPUResourceView.h"

#include "GPU/GPUBuffer.h"
#include "GPU/GPUResource.h"
#include "GPU/GPUSwapchain.h"
#include "GPU/GPUTexture.h"

#include "Core/Utility.h"

GPUResourceView::GPUResourceView(GPUResource&               resource,
                                 const GPUResourceViewDesc& desc) :
    GPUObject   (resource.GetDevice()),
    mResource   (&resource),
    mDesc       (desc)
{
    #if GEMINI_BUILD_DEBUG
        /* Only one usage flag should be set. */
        Assert(IsOnlyOneBitSet(GetUsage()));
        Assert(GetResource().GetUsage() & GetUsage());

        Assert(GetType() != kGPUResourceViewType_Buffer || GetFormat() == kPixelFormat_Unknown);
        Assert(GetType() == kGPUResourceViewType_Buffer || GetFormat() != kPixelFormat_Unknown);

        if (GetType() == kGPUResourceViewType_Buffer || GetType() == kGPUResourceViewType_TextureBuffer)
        {
            Assert(GetResource().IsBuffer());
            Assert(GetMipOffset() == 0 && GetMipCount() == 1);

            const auto& buffer = static_cast<const GPUBuffer&>(GetResource());

            Assert(GetElementOffset() + GetElementCount() <= buffer.GetSize());
        }
        else
        {
            Assert(GetResource().IsTexture());

            const auto& texture = static_cast<const GPUTexture&>(GetResource());

            Assert(GetUsage() == kGPUResourceUsage_ShaderRead || GetMipCount() == 1);
            Assert(GetMipOffset() + GetMipCount() <= texture.GetNumMipLevels());

            const bool isArray = GetType() == kGPUResourceViewType_Texture1DArray ||
                                 GetType() == kGPUResourceViewType_Texture2DArray ||
                                 GetType() == kGPUResourceViewType_TextureCubeArray;

            Assert(isArray || GetElementCount() == ((GetType() == kGPUResourceViewType_TextureCube) ? kGPUCubeFaceCount : 1));
            Assert(GetElementOffset() + GetElementCount() <= texture.GetArraySize());

            const bool isCube = GetType() == kGPUResourceViewType_TextureCube ||
                                GetType() == kGPUResourceViewType_TextureCubeArray;

            Assert(!isCube || ((GetElementOffset() % kGPUCubeFaceCount) == 0 && (GetElementCount() % kGPUCubeFaceCount) == 0));

            if (texture.IsSwapchain())
            {
                Assert(texture.GetSwapchain()->mIsInPresent);
                texture.GetSwapchain()->mViewCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
    #endif
}

GPUResourceView::~GPUResourceView()
{
    #if GEMINI_BUILD_DEBUG
        if (GetResource().IsTexture())
        {
            const auto& texture = static_cast<const GPUTexture&>(GetResource());

            if (texture.IsSwapchain())
            {
                texture.GetSwapchain()->mViewCount.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    #endif
}
