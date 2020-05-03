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

/**
 * Class managing shader constant data. We don't persist constant data across
 * frames, instead just rewrite what is needed each frame. Data is passed to
 * shaders by allocating a handle via this class, writing data to it, and then
 * specifying the handle to SetConstants() on the command list. Handles are only
 * valid within the current frame.
 */
class GPUConstantPool : public GPUDeviceChild
{
protected:
                            GPUConstantPool(GPUDevice& device);
                            ~GPUConstantPool();

public:
    /**
     * Allocate space for constant data, returning a handle to bind it later and
     * a mapping of the allocated space to write data to. This is free-threaded.
     */
    virtual GPUConstants    Allocate(const size_t size,
                                     void*&       outMapping) = 0;

    /**
     * Convenience wrapper to allocate constant data space and copy some data
     * into it.
     */
    GPUConstants            Write(const void* const data,
                                  const size_t      size);
};

inline GPUConstantPool::GPUConstantPool(GPUDevice& device) :
    GPUDeviceChild  (device)
{
}

inline GPUConstantPool::~GPUConstantPool()
{
}

inline GPUConstants GPUConstantPool::Write(const void* const data,
                                           const size_t      size)
{
    void* mapping;
    const GPUConstants handle = Allocate(size, mapping);

    memcpy(mapping, data, size);

    return handle;
}
