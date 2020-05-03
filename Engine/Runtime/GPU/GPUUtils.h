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

#include "GPU/GPUDefs.h"

namespace GPUUtils
{
    size_t              GetAttributeSize(const GPUAttributeFormat format);
    size_t              GetIndexSize(const GPUIndexType type);

    /**
     * Validate a resource state combination. Does not do anything on non-debug
     * builds.
     */
    void                ValidateResourceState(const GPUResourceState state,
                                              const bool             isTexture);
}

inline size_t GPUUtils::GetIndexSize(const GPUIndexType type)
{
    switch (type)
    {
        case kGPUIndexType_16: return 2;
        case kGPUIndexType_32: return 4;

        default:
            UnreachableMsg("Unknown index type");

    }
}

#if !GEMINI_BUILD_DEBUG

inline void GPUUtils::ValidateResourceState(const GPUResourceState state,
                                            const bool             isTexture)
{
}

#endif
