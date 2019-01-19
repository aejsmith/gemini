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

#include "GPU/GPUObject.h"

/** Base class for GPU resources (buffers and textures). */
class GPUResource : public GPUObject
{
protected:
                            GPUResource(GPUDevice&             inDevice,
                                        const GPUResourceType  inType,
                                        const GPUResourceUsage inUsage);

public:
                            ~GPUResource() {}

    GPUResourceType         GetType() const     { return mType; }
    GPUResourceUsage        GetUsage() const    { return mUsage; }

    bool                    IsTexture() const   { return mType != kGPUResourceType_Buffer; }
    bool                    IsBuffer() const    { return mType == kGPUResourceType_Buffer; }

    /**
     * Function called from GPUContext implementations to check that a barrier
     * is valid for this resource. A no-op on non-debug builds.
     */
    void                    ValidateBarrier(const GPUResourceBarrier& inBarrier) const;

private:
    const GPUResourceType   mType;
    const GPUResourceUsage  mUsage;

};

#if !ORION_BUILD_DEBUG

inline void GPUResource::ValidateBarrier(const GPUResourceBarrier& inBarrier) const
{
}

#endif
