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

#include "GPU/GPUDeviceChild.h"

struct GPUQueryPoolDesc
{
    GPUQueryType            type;
    uint16_t                count;
};

/**
 * A pool of GPU queries. Usage is to submit queries to the GPU via GPUContext
 * or GPUCommandList methods, wait a few frames for them to complete, then call
 * GetResults() to fetch results. Once used, queries must be reset before they
 * can be used again.
 */
class GPUQueryPool : public GPUDeviceChild
{
public:
    /** Behaviour flags for GetResults(). */
    enum GetResultsFlags : uint32_t
    {
        /** Wait until all results are available. */
        kGetResults_Wait    = 1 << 0,

        /** Reset queries after successfully fetching all results. */
        kGetResults_Reset   = 1 << 1,
    };

public:
    virtual                 ~GPUQueryPool() {}

    GPUQueryType            GetType() const     { return mDesc.type; }
    uint16_t                GetCount() const    { return mDesc.count; }

    /**
     * Reset a range of queries. Any use of the range must *not* be in flight
     * on the GPU - once GetResults() returns success for the whole range then
     * it is safe to reset.
     */
    virtual void            Reset(const uint16_t start,
                                  const uint16_t count) = 0;

    /**
     * Get results for a range of submitted queries. Returns whether the
     * results were available yet (always returns true if kGetResults_Wait is
     * set).
     */
    virtual bool            GetResults(const uint16_t start,
                                       const uint16_t count,
                                       const uint32_t flags,
                                       uint64_t*      outData) = 0;

protected:
                            GPUQueryPool(GPUDevice&              device,
                                         const GPUQueryPoolDesc& desc);

private:
    const GPUQueryPoolDesc  mDesc;

};
