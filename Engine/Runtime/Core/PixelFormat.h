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

#include "Core/CoreDefs.h"

enum PixelFormat : uint8_t
{
    /** Unknown format. */
    kPixelFormat_Unknown,

    /**
     * Colour formats. Note these are all given in the order of elements in
     * memory, independent of endianness.
     */
    kPixelFormat_R8G8B8A8,              /**< RGBA, unsigned normalized, 8 bits per component. */
    kPixelFormat_R8G8B8A8sRGB,          /**< RGBA, unsigned normalized, 8 bits per component, sRGB. */
    kPixelFormat_R8G8,                  /**< RG, unsigned normalized, 8 bits per component. */
    kPixelFormat_R8,                    /**< R, unsigned normalized, 8 bits per component. */
    kPixelFormat_B8G8R8A8,              /**< BGRA, unsigned normalized, 8 bits per component. */
    kPixelFormat_B8G8R8A8sRGB,          /**< BGRA, unsigned normalized, 8 bits per component, sRGB. */
    kPixelFormat_R10G10B10A2,           /**< RGBA, unsigned normalized, 10 bits RGB, 2 bits A. */
    kPixelFormat_FloatR16G16B16A16,     /**< RGBA, float, 16 bits per component. */
    kPixelFormat_FloatR16G16B16,        /**< RGB, float, 16 bits per component. */
    kPixelFormat_FloatR16G16,           /**< RG, float, 16 bits per component. */
    kPixelFormat_FloatR16,              /**< R, float, 16 bits per component. */
    kPixelFormat_FloatR32G32B32A32,     /**< RGBA, float, 32 bits per component. */
    kPixelFormat_FloatR32G32B32,        /**< RGB, float, 32 bits per component. */
    kPixelFormat_FloatR32G32,           /**< RG, float, 32 bits per component. */
    kPixelFormat_FloatR32,              /**< R, float, 32 bits per component. */

    /** Depth/stencil formats. */
    kPixelFormat_Depth16,               /**< Depth, 16 bits. */
    kPixelFormat_Depth32,               /**< Depth, 32 bits. */
    kPixelFormat_Depth32Stencil8,       /**< Depth/stencil, 24 bits/8 bits. */

    /** Number of pixel formats. */
    kPixelFormatCount,
};

class PixelFormatInfo
{
public:
    static bool                 IsColour(const PixelFormat inFormat);
    static bool                 IsSRGB(const PixelFormat inFormat);
    static bool                 IsFloat(const PixelFormat inFormat);
    static bool                 IsDepth(const PixelFormat inFormat);
    static bool                 IsDepthStencil(const PixelFormat inFormat);
    static size_t               BytesPerPixel(const PixelFormat inFormat);

    static PixelFormat          GetSRGBEquivalent(const PixelFormat inFormat);
    static PixelFormat          GetNonSRGBEquivalent(const PixelFormat inFormat);

private:
    struct Format
    {
        bool                    isSRGB;
        bool                    isFloat;
        bool                    isDepth;
        bool                    isDepthStencil;
        size_t                  bytesPerPixel;
    };

    static Format               sInfo[kPixelFormatCount];

};

inline bool PixelFormatInfo::IsColour(const PixelFormat inFormat)
{
    return !sInfo[inFormat].isDepth;
}

inline bool PixelFormatInfo::IsSRGB(const PixelFormat inFormat)
{
    return sInfo[inFormat].isSRGB;
}

inline bool PixelFormatInfo::IsFloat(const PixelFormat inFormat)
{
    return sInfo[inFormat].isFloat;
}

inline bool PixelFormatInfo::IsDepth(const PixelFormat inFormat)
{
    return sInfo[inFormat].isDepth;
}

inline bool PixelFormatInfo::IsDepthStencil(const PixelFormat inFormat)
{
    return sInfo[inFormat].isDepthStencil;
}

inline size_t PixelFormatInfo::BytesPerPixel(const PixelFormat inFormat)
{
    return sInfo[inFormat].bytesPerPixel;
}
