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

#include "Core/Hash.h"

#include "GPU/GPUObject.h"
#include "GPU/GPUSampler.h"

class GPUResourceView;

using GPUArgumentTypeArray = std::vector<GPUArgumentType>;

struct GPUArgumentSetLayoutDesc
{
    /**
     * Array of arguments. Index in the array matches the shader binding
     * index.
     */
    GPUArgumentTypeArray            arguments;

public:
                                    GPUArgumentSetLayoutDesc(const size_t inNumArguments = 0);

};

inline GPUArgumentSetLayoutDesc::GPUArgumentSetLayoutDesc(const size_t inNumArguments) :
    arguments   (inNumArguments)
{
}

inline size_t HashValue(const GPUArgumentSetLayoutDesc& inValue)
{
    return HashData(inValue.arguments.data(),
                    sizeof(inValue.arguments[0]) * inValue.arguments.size(),
                    inValue.arguments.size());
}

inline bool operator==(const GPUArgumentSetLayoutDesc& inA, const GPUArgumentSetLayoutDesc& inB)
{
    return inA.arguments.size() == inB.arguments.size() &&
           memcmp(inA.arguments.data(),
                  inB.arguments.data(),
                  sizeof(inA.arguments[0]) * inA.arguments.size()) == 0;
}

/**
 * Defines the layout of a set of arguments passed to a pipeline or compute
 * shader. Arguments are specified in sets with a fixed layout. Sets are bound
 * to a command list, which makes the arguments contained with them available
 * to shaders. The set index and argument index within them lines up with the
 * GLSL set/binding indices.
 *
 * When creating a pipeline, the argument set layouts expected by the pipeline
 * must be specified. When drawing with the pipeline, argument sets matching
 * the layout specified in the pipeline must be bound.
 */
class GPUArgumentSetLayout : public GPUDeviceChild
{
protected:
                                    GPUArgumentSetLayout(GPUDevice&                 inDevice,
                                                         GPUArgumentSetLayoutDesc&& inDesc);

                                    ~GPUArgumentSetLayout();

public:
    const GPUArgumentTypeArray&     GetArguments() const        { return mDesc.arguments; }
    uint8_t                         GetArgumentCount() const    { return mDesc.arguments.size(); }

    bool                            IsUniformOnly() const       { return mIsUniformOnly; }

private:
    const GPUArgumentSetLayoutDesc  mDesc;
    bool                            mIsUniformOnly;

    /* Allows the device to destroy cached layouts upon destruction. */
    friend class GPUDevice;
};

using GPUArgumentSetLayoutRef = const GPUArgumentSetLayout*;

struct GPUArgument
{
    GPUResourceView*                view    = nullptr;
    GPUSamplerRef                   sampler = nullptr;
};

/**
 * Persistent argument set, created with GPUDevice::CreateArgumentSet().
 * Creating sets persistently should be preferred over dynamically setting
 * arguments on a command list where possible, since it moves the overhead of
 * allocating space for and writing hardware descriptors from draw time to
 * creation time, and allows sets of arguments to be bound very cheaply.
 *
 * An exception to this is sets which only contain kGPUArgumentType_Uniforms
 * arguments: due to the way we handle uniforms, we can create these sets
 * dynamically very cheaply, e.g. on Vulkan we actually create just one set up
 * front at layout creation time and reuse that when asked to dynamically
 * create the set.
 *
 * Argument sets are immutable: if the higher level engine needs to change
 * arguments, it should create a new argument set.
 */
class GPUArgumentSet : public GPUObject
{
protected:
                                    GPUArgumentSet(GPUDevice&                    inDevice,
                                                   const GPUArgumentSetLayoutRef inLayout,
                                                   const GPUArgument* const      inArguments);

                                    ~GPUArgumentSet();

public:
    GPUArgumentSetLayoutRef         GetLayout() const { return mLayout; }

    static void                     ValidateArguments(const GPUArgumentSetLayoutRef inLayout,
                                                      const GPUArgument* const      inArguments);

private:
    const GPUArgumentSetLayoutRef   mLayout;

};

using GPUArgumentSetPtr = ReferencePtr<GPUArgumentSet>;

#ifndef ORION_BUILD_DEBUG

inline void GPUArgumentSet::ValidateArguments(const GPUArgumentSetLayoutRef inLayout,
                                              const GPUArgument* const      inArguments)
{
    /* On non-debug builds this does nothing. */
}

#endif
