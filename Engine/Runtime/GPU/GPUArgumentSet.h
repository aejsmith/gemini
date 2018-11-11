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

#include "GPU/GPUDeviceChild.h"

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
    const GPUArgumentTypeArray&     GetArguments() const { return mDesc.arguments; }

private:
    const GPUArgumentSetLayoutDesc  mDesc;

};
