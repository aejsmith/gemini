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

#include "GPU/GPUDeviceChild.h"

/**
 * Class managing shader uniform data. We don't persist uniform data across
 * frames, instead just rewrite what is needed each frame. Data is passed to
 * shaders by allocating a handle via this class, writing data to it, and then
 * specifying the handle to SetUniforms() on the command list. Handles are only
 * valid within the current frame.
 */
class GPUUniformPool : public GPUDeviceChild
{
protected:
                            GPUUniformPool(GPUDevice& inDevice);
                            ~GPUUniformPool();

public:
    /**
     * Allocate space for uniform data, returning a handle to bind it later and
     * a mapping of the allocated space to write data to. This is free-threaded.
     */
    virtual GPUUniforms     Allocate(const size_t inSize,
                                     void*&       outMapping) = 0;

    /**
     * Convenience wrapper to allocate uniform data space and copy some data
     * into it.
     */
    GPUUniforms             Write(const void* const inData,
                                  const size_t      inSize);
};

inline GPUUniformPool::GPUUniformPool(GPUDevice& inDevice) :
    GPUDeviceChild  (inDevice)
{
}

inline GPUUniformPool::~GPUUniformPool()
{
}

inline GPUUniforms GPUUniformPool::Write(const void* const inData,
                                         const size_t      inSize)
{
    void* mapping;
    const GPUUniforms handle = Allocate(inSize, mapping);

    memcpy(mapping, inData, inSize);

    return handle;
}
