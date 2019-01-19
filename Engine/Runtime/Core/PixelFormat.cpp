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

#include "Core/PixelFormat.h"

PixelFormat::Info PixelFormat::sInfo[PixelFormat::kNumFormats] =
{
    /* kUnknown */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 0,
    },

    /* kR8G8B8A8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kR8G8B8A8sRGB */
    {
        /* isSRGB         */ true,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kR8G8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 2,
    },

    /* kR8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 1,
    },

    /* kB8G8R8A8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kB8G8R8A8sRGB */
    {
        /* isSRGB         */ true,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kR10G10B10A2 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kFloatR16G16B16A16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 8,
    },

    /* kFloatR16G16B16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 6,
    },

    /* kFloatR16G16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kFloatR16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 2,
    },

    /* kFloatR32G32B32A32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 16,
    },

    /* kFloatR32G32B32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 12,
    },

    /* kFloatR32G32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 8,
    },

    /* kFloatR32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ true,
        /* isDepth        */ false,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kDepth16 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ true,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 2,
    },

    /* kDepth32 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ true,
        /* isDepthStencil */ false,
        /* bytesPerPixel  */ 4,
    },

    /* kDepth32Stencil8 */
    {
        /* isSRGB         */ false,
        /* isFloat        */ false,
        /* isDepth        */ true,
        /* isDepthStencil */ true,
        /* bytesPerPixel  */ 8,
    }
};

PixelFormat PixelFormat::GetSRGBEquivalent(const PixelFormat inFormat)
{
    switch (inFormat)
    {
        case PixelFormat::kR8G8B8A8:
            return PixelFormat::kR8G8B8A8sRGB;

        case PixelFormat::kB8G8R8A8:
            return PixelFormat::kB8G8R8A8sRGB;

        default:
            return inFormat;

    }
}

PixelFormat PixelFormat::GetNonSRGBEquivalent(const PixelFormat inFormat)
{
    switch (inFormat)
    {
        case PixelFormat::kR8G8B8A8sRGB:
            return PixelFormat::kR8G8B8A8;

        case PixelFormat::kB8G8R8A8sRGB:
            return PixelFormat::kB8G8R8A8;

        default:
            return inFormat;

    }
}
