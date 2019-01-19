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

class PixelFormat
{
public:
    enum Impl
    {
        /** Unknown format. */
        kUnknown = 0,

        /**
         * Colour formats. Note these are all given in the order of elements in
         * memory, independent of endianness.
         */
        kR8G8B8A8,              /**< RGBA, unsigned normalized, 8 bits per component. */
        kR8G8B8A8sRGB,          /**< RGBA, unsigned normalized, 8 bits per component, sRGB. */
        kR8G8,                  /**< RG, unsigned normalized, 8 bits per component. */
        kR8,                    /**< R, unsigned normalized, 8 bits per component. */
        kB8G8R8A8,              /**< BGRA, unsigned normalized, 8 bits per component. */
        kB8G8R8A8sRGB,          /**< BGRA, unsigned normalized, 8 bits per component, sRGB. */
        kR10G10B10A2,           /**< RGBA, unsigned normalized, 10 bits RGB, 2 bits A. */
        kFloatR16G16B16A16,     /**< RGBA, float, 16 bits per component. */
        kFloatR16G16B16,        /**< RGB, float, 16 bits per component. */
        kFloatR16G16,           /**< RG, float, 16 bits per component. */
        kFloatR16,              /**< R, float, 16 bits per component. */
        kFloatR32G32B32A32,     /**< RGBA, float, 32 bits per component. */
        kFloatR32G32B32,        /**< RGB, float, 32 bits per component. */
        kFloatR32G32,           /**< RG, float, 32 bits per component. */
        kFloatR32,              /**< R, float, 32 bits per component. */

        /** Depth/stencil formats. */
        kDepth16,               /**< Depth, 16 bits. */
        kDepth32,               /**< Depth, 32 bits. */
        kDepth32Stencil8,       /**< Depth/stencil, 24 bits/8 bits. */

        /** Number of pixel formats. */
        kNumFormats,
    };

    struct Info
    {
        bool                    isSRGB;
        bool                    isFloat;
        bool                    isDepth;
        bool                    isDepthStencil;
        size_t                  bytesPerPixel;
    };

public:
    constexpr                   PixelFormat() : mValue(kUnknown) {}
    constexpr                   PixelFormat(const Impl inValue) : mValue(inValue) {}
    constexpr                   operator Impl() const { return mValue; }

    static bool                 IsColour(const PixelFormat inFormat);
    static bool                 IsSRGB(const PixelFormat inFormat);
    static bool                 IsFloat(const PixelFormat inFormat);
    static bool                 IsDepth(const PixelFormat inFormat);
    static bool                 IsDepthStencil(const PixelFormat inFormat);
    static size_t               BytesPerPixel(const PixelFormat inFormat);

    static PixelFormat          GetSRGBEquivalent(const PixelFormat inFormat);
    static PixelFormat          GetNonSRGBEquivalent(const PixelFormat inFormat);

private:
    Impl                        mValue;

    static Info                 sInfo[kNumFormats];

};

inline bool PixelFormat::IsColour(const PixelFormat inFormat)
{
    return !sInfo[inFormat].isDepth;
}

inline bool PixelFormat::IsSRGB(const PixelFormat inFormat)
{
    return sInfo[inFormat].isSRGB;
}

inline bool PixelFormat::IsFloat(const PixelFormat inFormat)
{
    return sInfo[inFormat].isFloat;
}

inline bool PixelFormat::IsDepth(const PixelFormat inFormat)
{
    return sInfo[inFormat].isDepth;
}

inline bool PixelFormat::IsDepthStencil(const PixelFormat inFormat)
{
    return sInfo[inFormat].isDepthStencil;
}

inline size_t PixelFormat::BytesPerPixel(const PixelFormat inFormat)
{
    return sInfo[inFormat].bytesPerPixel;
}
