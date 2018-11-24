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

struct GPUBufferDesc
{
    /**
     * Usage flags for the buffer. This only needs to specify certain types of
     * usage which might require special handling, namely shader binding. All
     * buffers, regardless of usage, allow binding as vertex/index/indirect
     * buffers, and transfers.
     */
    GPUResourceUsage        usage = kGPUResourceUsage_Standard;

    uint32_t                size;
};

class GPUBuffer : public GPUResource
{
protected:
                            GPUBuffer(GPUDevice&           inDevice,
                                      const GPUBufferDesc& inDesc);

                            ~GPUBuffer() {}

public:
    uint32_t                GetSize() const { return mSize; }

private:
    const uint32_t          mSize;

};

using GPUBufferPtr = ReferencePtr<GPUBuffer>;
