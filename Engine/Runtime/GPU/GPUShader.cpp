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

#include "GPU/GPUShader.h"

#include "GPU/GPUDevice.h"

static std::atomic<GPUShaderID> gNextShaderID{0};

static GPUShaderID AllocateShaderID()
{
    const GPUShaderID id = gNextShaderID.fetch_add(1, std::memory_order_relaxed);

    if (id == std::numeric_limits<GPUShaderID>::max())
    {
        /* TODO: ID reuse. */
        Fatal("Ran out of shader IDs");
    }

    return id;
}

GPUShader::GPUShader(GPUDevice&           device,
                     const GPUShaderStage stage,
                     GPUShaderCode        code) :
    GPUObject   (device),
    mID         (AllocateShaderID()),
    mStage      (stage),
    mCode       (std::move(code))
{
}

GPUShader::~GPUShader()
{
    /* Removing the shader from the pipeline should trigger its destruction.
     * That will call back into RemovePipeline(). To prevent recursion, move
     * the pipeline set away. */
    auto pipelines = std::move(mPipelines);

    for (auto pipeline : pipelines)
    {
        GetDevice().DropPipeline(pipeline, {});
    }
}

void GPUShader::AddPipeline(GPUPipeline* const pipeline,
                            OnlyCalledBy<GPUDevice>)
{
    auto ret = mPipelines.emplace(pipeline);
    Assert(ret.second);
    Unused(ret);
}

void GPUShader::RemovePipeline(GPUPipeline* const pipeline,
                               OnlyCalledBy<GPUPipeline>)
{
    mPipelines.erase(pipeline);
}
