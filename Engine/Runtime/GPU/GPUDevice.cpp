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

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"

#include "Vulkan/VulkanDevice.h"

#include <mutex>

SINGLETON_IMPL(GPUDevice);

GPUDevice::GPUDevice() :
    mVendor          (kGPUVendor_Unknown),
    mGraphicsContext (nullptr)
{
}

GPUDevice::~GPUDevice()
{
}

void GPUDevice::Create()
{
    /* For now, only Vulkan. This will initialise the Singleton pointer. */
    new VulkanDevice();
}

void GPUDevice::EndFrame()
{
    #if ORION_BUILD_DEBUG
        Assert(mGraphicsContext->mActivePassCount == 0);
        //Assert(!mComputeContext || mComputeContext->mActivePassCount == 0);
    #endif

    EndFrameImpl();
}

GPUPipeline* GPUDevice::GetPipelineImpl(const GPUPipelineDesc& inDesc)
{
    /*
     * Pre-created pipelines go through the cache used for dynamically created
     * pipelines. This is to avoid potentially duplicating pipelines if we have
     * the same pipeline pre-created and dynamically created.
     *
     * Pipelines are destroyed when any shader they refer to is destroyed. To
     * prevent pre-created pipelines from being destroyed while any reference
     * is still held to them, when the reference count of a pipeline is non-
     * zero it holds a reference to its shaders. When the reference count of
     * a pipeline drops to zero, we release those references to the shaders,
     * but do not destroy the pipeline. Releasing the shaders may trigger
     * destruction of the pipeline, but if not, the pipeline is kept around in
     * the cache for later use.
     */

    const size_t hash = HashValue(inDesc);

    /* Check whether we have a copy of the descriptor stored. Lock for reading
     * to begin with. */
    GPUPipeline* pipeline = nullptr;

    {
        std::shared_lock lock(mPipelineCacheLock);

        auto it = mPipelineCache.find(hash);
        if (it != mPipelineCache.end())
        {
            pipeline = it->second;

            /* Sanity check that we aren't getting any hash collisions. */
            Assert(inDesc == pipeline->GetDesc());
        }
    }

    if (!pipeline)
    {
        /* Pipeline creation may take a long time, do it outside the lock to
         * allow parallel creation of pipelines on other threads. */
        pipeline = CreatePipelineImpl(inDesc);

        Assert(pipeline->GetRefCount() == 0);

        std::unique_lock lock(mPipelineCacheLock);

        auto ret = mPipelineCache.emplace(hash, pipeline);

        if (ret.second)
        {
            /* Register with the shaders. mPipelineCacheLock guards the shader
             * pipeline sets. */
            for (auto shader : inDesc.shaders)
            {
                if (shader)
                {
                    shader->AddPipeline(pipeline, {});
                }
            }
        }
        else
        {
            /* Another thread created the same pipeline and beat us to adding
             * it to the cache. Use that one instead. */
            pipeline->Destroy({});
            pipeline = ret.first->second;

            Assert(inDesc == pipeline->GetDesc());
        }
    }

    return pipeline;
}

void GPUDevice::DropPipeline(GPUPipeline* const inPipeline,
                             OnlyCalledBy<GPUShader>)
{
    /* If the shader is being destroyed, we shouldn't have any references
     * left on the pipeline, since whenver the pipeline refcount is non-zero,
     * a reference is held to the shaders. */
    Assert(inPipeline->GetRefCount() == 0);

    const size_t hash = HashValue(inPipeline->GetDesc());

    std::unique_lock lock(mPipelineCacheLock);

    auto ret = mPipelineCache.find(hash);
    Assert(ret != mPipelineCache.end());
    Assert(ret->second == inPipeline);

    mPipelineCache.erase(ret);

    /* Destroy the pipeline. We must do this with the lock held, since the lock
     * guards access to all shaders' mPipelines set. */
    inPipeline->Destroy({});
}

GPUPipelinePtr GPUDevice::CreatePipeline(const GPUPipelineDesc& inDesc)
{
    /* See GetPipelineImpl() for details. */
    GPUPipeline* const pipeline = GetPipelineImpl(inDesc);
    return pipeline->CreatePtr({});
}
