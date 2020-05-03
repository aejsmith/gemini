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

inline bool operator==(const GPUBufferDesc& a, const GPUBufferDesc& b)
{
    return a.usage == b.usage &&
           a.size  == b.size;
}

class GPUBuffer : public GPUResource
{
protected:
                            GPUBuffer(GPUDevice&           device,
                                      const GPUBufferDesc& desc);

public:
                            ~GPUBuffer() {}

    uint32_t                GetSize() const { return mSize; }

    GPUSubresourceRange     GetSubresourceRange() const override;

private:
    const uint32_t          mSize;

};
