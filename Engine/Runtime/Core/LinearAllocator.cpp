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

#include "Core/LinearAllocator.h"

#include "Core/Utility.h"

#include <cstdlib>

static const size_t kMinAlignment = 16;
static const size_t kMaxAlignment = 4096;

LinearAllocator::LinearAllocator(const size_t inMaxSize) :
    mCurrentOffset  (0),
    mMaxSize        (inMaxSize)
{
    mAllocation = std::aligned_alloc(kMaxAlignment, inMaxSize);
}

LinearAllocator::~LinearAllocator()
{
    std::free(mAllocation);
}

void* LinearAllocator::Allocate(const size_t inSize,
                                const size_t inAlignment)
{
    Assert(IsPowerOf2(inAlignment));
    Assert(inAlignment <= kMaxAlignment);

    /* Ensure that mCurrentOffset is always aligned to kMinAlignment. */
    const size_t size = RoundUp(inSize, kMinAlignment);

    size_t offset;

    if (inAlignment > kMinAlignment)
    {
        /* When we have an alignment greater than the minimum, use a CAS loop
         * to get and update the current offset, because we need to make sure
         * that the offset is aligned and advanced by enough to cover the
         * alignment and the allocation size. */
        size_t currentOffset = mCurrentOffset.load(std::memory_order_relaxed);
        size_t newOffset;

        do
        {
            offset    = RoundUp(currentOffset, inAlignment);
            newOffset = offset + size;
        }
        while (!mCurrentOffset.compare_exchange_strong(currentOffset,
                                                       newOffset,
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed));
    }
    else
    {
        /* Otherwise we can just use a simple atomic add. */
        offset = mCurrentOffset.fetch_add(size, std::memory_order_relaxed);
    }

    if (offset + size > mMaxSize)
    {
        /* TODO: Make this automatically expand. */
        Fatal("LinearAllocator allocation of %zu bytes exceeded maximum size %zu",
              inSize,
              mMaxSize);
    }

    return reinterpret_cast<uint8_t*>(mAllocation) + offset;
}

void LinearAllocator::Reset()
{
    #if ORION_BUILD_DEBUG
        if (mOutstandingDeletions.load(std::memory_order_relaxed))
        {
            Fatal("LinearAllocator still has undeleted allocations at reset");
        }
    #endif

    mCurrentOffset.store(0, std::memory_order_relaxed);
}
