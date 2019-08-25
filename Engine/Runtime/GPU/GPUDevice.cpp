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

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUSampler.h"

#include "Vulkan/VulkanDevice.h"

#include <mutex>

SINGLETON_IMPL(GPUDevice);

GPUDevice::GPUDevice() :
    mVendor             (kGPUVendor_Unknown),
    mGraphicsContext    (nullptr),
    mStagingPool        (nullptr),
    mConstantPool       (nullptr)
{
}

GPUDevice::~GPUDevice()
{
}

void GPUDevice::DestroyResources()
{
    for (auto& it : mSamplerCache)
    {
        delete it.second;
    }

    for (auto& it : mArgumentSetLayoutCache)
    {
        delete it.second;
    }

    /* All externally created resources should have been destroyed (explicitly
     * created pipelines, and shaders), therefore this should already be empty. */
    Assert(mPipelineCache.empty());
}

void GPUDevice::Create()
{
    /* For now, only Vulkan. This will initialise the Singleton pointer. */
    new VulkanDevice();
}

void GPUDevice::EndFrame()
{
    Assert(Thread::IsMain());

    #if ORION_BUILD_DEBUG
        Assert(mGraphicsContext->mActivePassCount == 0);
        //Assert(!mComputeContext || mComputeContext->mActivePassCount == 0);
    #endif

    EndFrameImpl();
}

GPUArgumentSetLayoutRef GPUDevice::GetArgumentSetLayout(GPUArgumentSetLayoutDesc&& inDesc)
{
    const size_t hash = HashValue(inDesc);

    GPUArgumentSetLayout* layout = nullptr;

    {
        std::shared_lock lock(mResourceCacheLock);

        auto it = mArgumentSetLayoutCache.find(hash);
        if (it != mArgumentSetLayoutCache.end())
        {
            layout = it->second;
        }
    }

    if (!layout)
    {
        layout = CreateArgumentSetLayoutImpl(std::move(inDesc));

        std::unique_lock lock(mResourceCacheLock);

        auto ret = mArgumentSetLayoutCache.emplace(hash, layout);

        if (!ret.second)
        {
            delete layout;
            layout = ret.first->second;
        }
    }

    return layout;
}

GPUPipeline* GPUDevice::GetPipeline(const GPUPipelineDesc& inDesc)
{
    const size_t hash = HashValue(inDesc);

    /* Check whether we have a copy of the descriptor stored. Lock for reading
     * to begin with. */
    GPUPipeline* pipeline = nullptr;

    {
        std::shared_lock lock(mResourceCacheLock);

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

        std::unique_lock lock(mResourceCacheLock);

        auto ret = mPipelineCache.emplace(hash, pipeline);

        if (ret.second)
        {
            /* Register with the shaders. mResourceCacheLock guards the shader
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
    const size_t hash = HashValue(inPipeline->GetDesc());

    std::unique_lock lock(mResourceCacheLock);

    auto ret = mPipelineCache.find(hash);
    Assert(ret != mPipelineCache.end());
    Assert(ret->second == inPipeline);

    mPipelineCache.erase(ret);

    /* Destroy the pipeline. We must do this with the lock held, since the lock
     * guards access to all shaders' mPipelines set. */
    inPipeline->Destroy({});
}

GPUSamplerRef GPUDevice::GetSampler(const GPUSamplerDesc& inDesc)
{
    const size_t hash = HashValue(inDesc);

    GPUSampler* layout = nullptr;

    {
        std::shared_lock lock(mResourceCacheLock);

        auto it = mSamplerCache.find(hash);
        if (it != mSamplerCache.end())
        {
            layout = it->second;
        }
    }

    if (!layout)
    {
        layout = CreateSamplerImpl(inDesc);

        std::unique_lock lock(mResourceCacheLock);

        auto ret = mSamplerCache.emplace(hash, layout);

        if (!ret.second)
        {
            delete layout;
            layout = ret.first->second;
        }
    }

    return layout;
}
