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

#include "Core/LinearAllocator.h"
#include "Core/Utility.h"

class Engine;

/**
 * This class implements a per-frame temporary allocator. All allocations made
 * with it will only last until the end of the current frame, after which the
 * allocator will be reset and the memory reused for the next frame.
 *
 * Allocations of plain memory or trivially destructible types can be done with
 * the Allocate() methods. These allocations do not need to be explicitly freed,
 * and will be automatically freed at end of frame.
 *
 * Non-trivially-destructible types need to be allocated with New(), and
 * explicitly destroyed with Delete(). This is to ensure that the destructor
 * gets run, in case it has any effects that would cause problems if not done.
 * Debug builds will verify at end of frame that there are no outstanding
 * allocations by New() that haven't been Delete()'d.
 */
class FrameAllocator
{
public:
    static void*                Allocate(const size_t size,
                                         const size_t alignment = 0);

    template <typename T, typename... Args>
    static T*                   Allocate(Args&&... args);

    template <typename T>
    static T*                   AllocateArray(const uint32_t count);

    template <typename T, typename... Args>
    static T*                   New(Args&&... args);

    template <typename T>
    static void                 Delete(T* const object);

    static void                 EndFrame(OnlyCalledBy<Engine>);

private:
                                FrameAllocator() {}
                                ~FrameAllocator() {}

private:
    static LinearAllocator      mAllocator;

};

inline void* FrameAllocator::Allocate(const size_t size,
                                      const size_t alignment)
{
    return mAllocator.Allocate(size, alignment);
}

template <typename T, typename... Args>
inline T* FrameAllocator::Allocate(Args&&... args)
{
    return mAllocator.Allocate<T>(std::forward<Args>(args)...);
}

template <typename T>
inline T* FrameAllocator::AllocateArray(const uint32_t count)
{
    return mAllocator.AllocateArray<T>(count);
}

template <typename T, typename... Args>
inline T* FrameAllocator::New(Args&&... args)
{
    return mAllocator.New<T>(std::forward<Args>(args)...);
}

template <typename T>
inline void FrameAllocator::Delete(T* const object)
{
    mAllocator.Delete<T>(object);
}

inline void FrameAllocator::EndFrame(OnlyCalledBy<Engine>)
{
    mAllocator.Reset();
}
