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

#include "Core/PixelFormat.h"

PixelFormatInfo::Format PixelFormatInfo::sInfo[kPixelFormatCount] =
{
    /* kPixelFormat_Unknown */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 0,
    },

    /* kPixelFormat_R8G8B8A8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_R8G8B8A8sRGB */
    {
        /* isSRGB         */ true,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_R8G8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 2,
    },

    /* kPixelFormat_R8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 1,
    },

    /* kPixelFormat_B8G8R8A8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_B8G8R8A8sRGB */
    {
        /* isSRGB         */ true,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_R10G10B10A2 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_FloatR11G11B10 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_FloatR16G16B16A16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 8,
    },

    /* kPixelFormat_FloatR16G16B16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 6,
    },

    /* kPixelFormat_FloatR16G16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_FloatR16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 2,
    },

    /* kPixelFormat_FloatR32G32B32A32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 16,
    },

    /* kPixelFormat_FloatR32G32B32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 12,
    },

    /* kPixelFormat_FloatR32G32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 8,
    },

    /* kPixelFormat_FloatR32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_Depth16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ true,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 2,
    },

    /* kPixelFormat_Depth32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ true,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kPixelFormat_Depth32Stencil8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ true,
        /* isDepthStencil */ true,
        /* bytesPerPixel  */ 8,
    }
};

PixelFormat PixelFormatInfo::GetSRGBEquivalent(const PixelFormat format)
{
    switch (format)
    {
        case kPixelFormat_R8G8B8A8:
            return kPixelFormat_R8G8B8A8sRGB;

        case kPixelFormat_B8G8R8A8:
            return kPixelFormat_B8G8R8A8sRGB;

        default:
            return format;

    }
}

PixelFormat PixelFormatInfo::GetNonSRGBEquivalent(const PixelFormat format)
{
    switch (format)
    {
        case kPixelFormat_R8G8B8A8sRGB:
            return kPixelFormat_R8G8B8A8;

        case kPixelFormat_B8G8R8A8sRGB:
            return kPixelFormat_B8G8R8A8;

        default:
            return format;

    }
}
