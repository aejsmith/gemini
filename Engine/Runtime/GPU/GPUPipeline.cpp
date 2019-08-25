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

#include "GPU/GPUPipeline.h"

static std::atomic<GPUPipelineID> gNextPipelineID{0};

static GPUPipelineID AllocatePipelineID()
{
    const GPUPipelineID id = gNextPipelineID.fetch_add(1, std::memory_order_relaxed);

    if (id == std::numeric_limits<GPUPipelineID>::max())
    {
        /* TODO: ID reuse. */
        Fatal("Ran out of pipeline IDs");
    }

    return id;
}

GPUPipeline::GPUPipeline(GPUDevice&             inDevice,
                         const GPUPipelineDesc& inDesc) :
    GPUObject   (inDevice),
    mDesc       (inDesc),
    mID         (AllocatePipelineID())
{
    for (size_t stage = 0; stage < kGPUShaderStage_NumGraphics; stage++)
    {
        mShaderIDs[stage] = (mDesc.shaders[stage])
                                ? mDesc.shaders[stage]->GetID()
                                : std::numeric_limits<GPUShaderID>::max();
    }
}

GPUPipeline::~GPUPipeline()
{
    for (auto shader : mDesc.shaders)
    {
        if (shader)
        {
            shader->RemovePipeline(this, {});
        }
    }
}

GPUPipelinePtr GPUPipeline::CreatePtr(OnlyCalledBy<GPUDevice>)
{
    /* See GPUDevice::GetPipelineImpl() for details. This is called from
     * CreatePipeline() to reference the shaders if this is the initial public
     * reference to the pipeline. */
    if (Retain() == 1)
    {
        for (auto shader : mDesc.shaders)
        {
            if (shader)
            {
                shader->Retain();
            }
        }
    }

    /* Take over the reference we just added. */
    GPUPipelinePtr ptr;
    ptr.Reset(this, false);
    return ptr;
}

void GPUPipeline::Released()
{
    /* We are no longer publically acessible, drop the shader references. This
     * may trigger our destruction if a shader gets destroyed, otherwise we
     * remain in the cache for later use, until one of the shaders does get
     * destroyed. */
    for (auto shader : mDesc.shaders)
    {
        if (shader)
        {
            shader->Release();
        }
    }
}

void GPUPipeline::Destroy(OnlyCalledBy<GPUDevice>)
{
    delete this;
}

GPUComputePipeline::GPUComputePipeline(GPUDevice&                    inDevice,
                                       const GPUComputePipelineDesc& inDesc) :
    GPUObject   (inDevice),
    mDesc       (inDesc)
{
    Assert(mDesc.shader);
    Assert(mDesc.shader->GetStage() == kGPUShaderStage_Compute);

    mDesc.shader->Retain();
}

GPUComputePipeline::~GPUComputePipeline()
{
    mDesc.shader->Release();
}
