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

#include "GPU/GPUContext.h"

#include "VulkanDeviceChild.h"

#include <list>
#include <mutex>
#include <vector>

class VulkanCommandPool;

/**
 * Vulkan implementation of GPU*Context. We have just a single implementation
 * of the most derived class and use this for all, to keep things a bit more
 * simple.
 */
class VulkanContext final : public GPUGraphicsContext,
                            public VulkanDeviceChild<VulkanContext>
{
public:
                                    VulkanContext(VulkanDevice&  device,
                                                  const uint8_t  id,
                                                  const uint32_t queueFamily);

                                    ~VulkanContext();

    /**
     * GPUContext methods.
     */
public:
    void                            Wait(GPUContext& otherContext) override;

    /**
     * GPUTransferContext methods.
     */
public:
    void                            ResourceBarrier(const GPUResourceBarrier* const barriers,
                                                    const size_t                    count) override;

    void                            BlitTexture(GPUTexture* const    destTexture,
                                                const GPUSubresource destSubresource,
                                                const glm::ivec3&    destOffset,
                                                const glm::ivec3&    destSize,
                                                GPUTexture* const    sourceTexture,
                                                const GPUSubresource sourceSubresource,
                                                const glm::ivec3&    sourceOffset,
                                                const glm::ivec3&    sourceSize) override;

    void                            ClearTexture(GPUTexture* const          texture,
                                                 const GPUTextureClearData& data,
                                                 const GPUSubresourceRange& range) override;

    void                            UploadBuffer(GPUBuffer* const        destBuffer,
                                                 const GPUStagingBuffer& sourceBuffer,
                                                 const uint32_t          size,
                                                 const uint32_t          destOffset,
                                                 const uint32_t          sourceOffset) override;

    void                            UploadTexture(GPUTexture* const        destTexture,
                                                  const GPUStagingTexture& sourceTexture) override;
    void                            UploadTexture(GPUTexture* const        destTexture,
                                                  const GPUSubresource     destSubresource,
                                                  const glm::ivec3&        destOffset,
                                                  const GPUStagingTexture& sourceTexture,
                                                  const GPUSubresource     sourceSubresource,
                                                  const glm::ivec3&        sourceOffset,
                                                  const glm::ivec3&        size) override;

    #if GEMINI_GPU_MARKERS

    void                            BeginMarker(const char* const label) override;
    void                            EndMarker() override;

    #endif

    /**
     * GPUComputeContext methods.
     */
public:
    void                            BeginPresent(GPUSwapchain& swapchain) override;
    void                            EndPresent(GPUSwapchain& swapchain) override;

protected:
    GPUComputeCommandList*          CreateComputePassImpl() override;
    void                            SubmitComputePassImpl(GPUComputeCommandList* const cmdList) override;

    /**
     * GPUGraphicsContext methods.
     */
protected:
    GPUGraphicsCommandList*         CreateRenderPassImpl(const GPURenderPass& renderPass) override;
    void                            SubmitRenderPassImpl(GPUGraphicsCommandList* const cmdList) override;

    /**
     * Internal methods.
     */
public:
    VkQueue                         GetQueue() const            { return mQueue; }
    uint32_t                        GetQueueFamily() const      { return mQueueFamily; }

    /** Gets the command pool for the current thread on the current frame. */
    VulkanCommandPool&              GetCommandPool();

    void                            BeginFrame();
    void                            EndFrame();

private:
    /**
     * Get the current primary command buffer. allocate specifies whether to
     * allocate a new command buffer if one is not currently being recorded -
     * this should only be done if we're actually going to record a command,
     * to avoid submitting empty command buffers.
     */
    VkCommandBuffer                 GetCommandBuffer(const bool allocate = true);

    bool                            HaveCommandBuffer() const   { return mCommandBuffer != VK_NULL_HANDLE; }

    /**
     * If we have a command buffer, submit it. If not null, the specified
     * semaphore will be signalled (even if there is no current command buffer).
     */
    void                            Submit(const VkSemaphore signalSemaphore = VK_NULL_HANDLE);

    /**
     * Wait for a semaphore. Any subsequent GPU work on the context will wait
     * until the semaphore has been signalled. If we currently have unsubmitted
     * work, it will be submitted.
     */
    void                            Wait(const VkSemaphore semaphore);

private:
    uint8_t                         mID;
    VkQueue                         mQueue;
    uint32_t                        mQueueFamily;

    /** Primary command buffer. This is only recorded by the main thread. */
    VkCommandBuffer                 mCommandBuffer;

    /**
     * Semaphores to wait on in the next submission. Even though our Wait()
     * function only takes a single semaphore, this is an array because multiple
     * Wait() calls without any commands in between should make the next
     * submission wait on all of those.
     */
    std::vector<VkSemaphore>        mWaitSemaphores;

    /**
     * Command pools created for this context, per-frame. There can be an
     * arbitrary number of pools for a frame (for every thread which records
     * command lists that will be submitted to the context). Note these are
     * usually accessed through the thread-local sCommandPools array in
     * VulkanContext.cpp - the purpose of this array is to be able to reset
     * all command pools at end of frame.
     */
    std::mutex                      mCommandPoolsLock;
    std::list<VulkanCommandPool*>   mCommandPools[kVulkanInFlightFrameCount];

};
