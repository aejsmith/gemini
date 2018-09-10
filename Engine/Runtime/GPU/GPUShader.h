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

#include "Core/HashTable.h"
#include "Core/String.h"

#include "GPU/GPUObject.h"

class GPUPipeline;

class GPUShader : public GPUObject
{
protected:
                                GPUShader(GPUDevice&           inDevice,
                                          const GPUShaderStage inStage,
                                          GPUShaderCode        inCode);

                                ~GPUShader();

public:
    GPUShaderStage              GetStage() const    { return mStage; }
    const GPUShaderCode&        GetCode() const     { return mCode; }

    /** Interface with GPUDevice/GPUPipeline to register pipelines to shaders. */
    void                        AddPipeline(GPUPipeline* const inPipeline,
                                            OnlyCalledBy<GPUDevice>);
    void                        RemovePipeline(GPUPipeline* const inPipeline,
                                               OnlyCalledBy<GPUPipeline>);

private:
    const GPUShaderStage        mStage;
    const GPUShaderCode         mCode;

    /**
     * Pipelines which refer to this shader, to allow destruction of pipelines
     * when the shader is destroyed. This is accessed under the guard of the
     * device's mPipelineCacheLock.
     */
    HashSet<GPUPipeline*>       mPipelines;

};

using GPUShaderPtr = ReferencePtr<GPUShader>;
