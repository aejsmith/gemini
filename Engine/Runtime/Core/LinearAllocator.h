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

#include <atomic>
#include <new>
#include <utility>

/**
 * This class implements a thread safe linear allocator. Individual allocations
 * from it cannot be freed - only all allocations can be freed at once by
 * resetting the allocator.
 *
 * Allocations of plain memory or trivially destructible types can be done with
 * the Allocate() methods.
 *
 * Non-trivially-destructible types need to be allocated with New(), and
 * explicitly destroyed with Delete(). This is to ensure that the destructor
 * gets run, in case it has any effects that would cause problems if not done.
 * Debug builds will verify at reset time that there are no outstanding
 * allocations by New() that haven't been Delete()'d.
 */
class LinearAllocator
{
public:
                                LinearAllocator(const size_t maxSize);
                                ~LinearAllocator();

    void*                       Allocate(const size_t size,
                                         const size_t alignment = 0);

    template <typename T, typename... Args>
    T*                          Allocate(Args&&... args);

    template <typename T>
    T*                          AllocateArray(const uint32_t count);

    template <typename T, typename... Args>
    T*                          New(Args&&... args);

    template <typename T>
    void                        Delete(T* const object);

    void                        Reset();

private:
    void*                       mAllocation;
    std::atomic<size_t>         mCurrentOffset;
    size_t                      mMaxSize;

    #if GEMINI_BUILD_DEBUG
    std::atomic<uint32_t>       mOutstandingDeletions;
    #endif
};

template <typename T, typename... Args>
inline T* LinearAllocator::Allocate(Args&&... args)
{
    static_assert(!std::is_array<T>::value,
                  "T must not be an array type");
    static_assert(std::is_trivially_destructible<T>::value,
                  "T must be trivially destructible - use New()/Delete() instead");

    void* const allocation = Allocate(sizeof(T), alignof(T));

    return new (allocation) T(std::forward<Args>(args)...);
}

template <typename T>
inline T* LinearAllocator::AllocateArray(const uint32_t count)
{
    static_assert(!std::is_array<T>::value,
                  "T must not be an array type");
    static_assert(std::is_trivially_destructible<T>::value,
                  "T must be trivially destructible - use New()/Delete() instead");

    void* const allocation = Allocate(sizeof(T) * count, alignof(T));

    for (uint32_t i = 0; i < count; i++)
    {
        new (reinterpret_cast<uint8_t*>(allocation) + (i * sizeof(T))) T;
    }

    return reinterpret_cast<T*>(allocation);
}

template <typename T, typename... Args>
inline T* LinearAllocator::New(Args&&... args)
{
    static_assert(!std::is_array<T>::value,
                  "T must not be an array type");
    static_assert(!std::is_trivially_destructible<T>::value,
                  "T must not be trivially destructible - use Allocate() instead");

    #if GEMINI_BUILD_DEBUG
        mOutstandingDeletions.fetch_add(1, std::memory_order_relaxed);
    #endif

    void* const allocation = Allocate(sizeof(T), alignof(T));

    return new (allocation) T(std::forward<Args>(args)...);
}

template <typename T>
inline void LinearAllocator::Delete(T* const object)
{
    object->~T();

    #if GEMINI_BUILD_DEBUG
        mOutstandingDeletions.fetch_sub(1, std::memory_order_relaxed);
    #endif
}
