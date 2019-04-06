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

#include "VulkanFormat.h"

static VkFormat sFormatTable[kPixelFormatCount] =
{
    /* kPixelFormat_Unknown */              VK_FORMAT_UNDEFINED,
    /* kPixelFormat_R8G8B8A8 */             VK_FORMAT_R8G8B8A8_UNORM,
    /* kPixelFormat_R8G8B8A8sRGB */         VK_FORMAT_R8G8B8A8_SRGB,
    /* kPixelFormat_R8G8 */                 VK_FORMAT_R8G8_UNORM,
    /* kPixelFormat_R8 */                   VK_FORMAT_R8_UNORM,
    /* kPixelFormat_B8G8R8A8 */             VK_FORMAT_B8G8R8A8_UNORM,
    /* kPixelFormat_B8G8R8A8sRGB */         VK_FORMAT_B8G8R8A8_SRGB,
    /* kPixelFormat_R10G10B10A2 */          VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    /* kPixelFormat_FloatR16G16B16A16 */    VK_FORMAT_R16G16B16A16_SFLOAT,
    /* kPixelFormat_FloatR16G16B16 */       VK_FORMAT_R16G16B16_SFLOAT,
    /* kPixelFormat_FloatR16G16 */          VK_FORMAT_R16G16_SFLOAT,
    /* kPixelFormat_FloatR16 */             VK_FORMAT_R16_SFLOAT,
    /* kPixelFormat_FloatR32G32B32A32 */    VK_FORMAT_R32G32B32A32_SFLOAT,
    /* kPixelFormat_FloatR32G32B32 */       VK_FORMAT_R32G32B32_SFLOAT,
    /* kPixelFormat_FloatR32G32 */          VK_FORMAT_R32G32_SFLOAT,
    /* kPixelFormat_FloatR32 */             VK_FORMAT_R32_SFLOAT,
    /* kPixelFormat_Depth16 */              VK_FORMAT_D16_UNORM,
    /* kPixelFormat_Depth32 */              VK_FORMAT_D32_SFLOAT,
    /* kPixelFormat_Depth32Stencil8 */      VK_FORMAT_D32_SFLOAT_S8_UINT,
};

VkFormat VulkanFormat::GetVulkanFormat(const PixelFormat inFormat)
{
    Assert(inFormat < kPixelFormatCount);
    return sFormatTable[inFormat];
}

PixelFormat VulkanFormat::GetPixelFormat(const VkFormat inFormat)
{
    for (int i = 0; i < kPixelFormatCount; i++)
    {
        if (sFormatTable[i] == inFormat)
        {
            return static_cast<PixelFormat>(i);
        }
    }

    return kPixelFormat_Unknown;
}
