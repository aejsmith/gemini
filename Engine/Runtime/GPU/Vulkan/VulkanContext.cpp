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

#include "VulkanContext.h"

#include "VulkanCommandPool.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"

/**
 * Per-thread, per-context, per-frame command pools. We have separate pools for
 * each in-flight frame. When a frame is completed, all pools are reset and
 * then re-used for the next frame.
 */
static thread_local VulkanCommandPool* sCommandPools[kVulkanMaxContexts][kVulkanInFlightFrameCount] = {{}};

VulkanContext::VulkanContext(VulkanDevice&  inDevice,
                             const uint8_t  inID,
                             const uint32_t inQueueFamily) :
    GPUGraphicsContext  (inDevice),
    mID                 (inID),
    mQueueFamily        (inQueueFamily),
    mCommandBuffer      (VK_NULL_HANDLE)
{
    vkGetDeviceQueue(GetVulkanDevice().GetHandle(),
                     mQueueFamily,
                     0,
                     &mQueue);
}

VulkanContext::~VulkanContext()
{
    for (size_t i = 0; i < ArraySize(mCommandPools); i++)
    {
        for (VulkanCommandPool* commandPool : mCommandPools[i])
        {
            delete commandPool;
        }
    }
}

VulkanCommandPool& VulkanContext::GetCommandPool()
{
    const uint8_t frame = GetVulkanDevice().GetCurrentFrame();

    VulkanCommandPool*& pool = sCommandPools[mID][frame];

    /* Check if we have a pool for this thread yet. FIXME: This won't cope with
     * the device being destroyed and created again (pointers will no longer be
     * null but need to be re-created). */
    if (Unlikely(!pool))
    {
        pool = new VulkanCommandPool(GetVulkanDevice(), mQueueFamily);

        std::unique_lock lock(mCommandPoolsLock);
        mCommandPools[frame].emplace_back(pool);
    }

    return *pool;
}

VkCommandBuffer VulkanContext::GetCommandBuffer(const bool inAllocate)
{
    if (mCommandBuffer == VK_NULL_HANDLE && inAllocate)
    {
        /* We do not have a command buffer, so allocate one and begin it. */
        mCommandBuffer = GetCommandPool().AllocatePrimary();

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VulkanCheck(vkBeginCommandBuffer(mCommandBuffer, &beginInfo));
    }

    return mCommandBuffer;
}

void VulkanContext::Submit(const VkSemaphore inSignalSemaphore)
{
    /* Don't need to do anything if we have nothing to submit and don't have a
     * semaphore to signal. */
    if (!HaveCommandBuffer() && inSignalSemaphore == VK_NULL_HANDLE)
    {
        return;
    }

    /* Wait for all semaphores at top of pipe for now. TODO: Perhaps we can
     * optimise this, e.g. a swapchain acquire wait probably can just wait at
     * the stage that accesses the image. */
    auto waitStages = AllocateStackArray(VkPipelineStageFlags, mWaitSemaphores.size());
    std::fill_n(waitStages, mWaitSemaphores.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = mWaitSemaphores.size();
    submitInfo.pWaitSemaphores      = mWaitSemaphores.data();
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = (mCommandBuffer != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pCommandBuffers      = (mCommandBuffer != VK_NULL_HANDLE) ? &mCommandBuffer : nullptr;
    submitInfo.signalSemaphoreCount = (inSignalSemaphore != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pSignalSemaphores    = (inSignalSemaphore != VK_NULL_HANDLE) ? &inSignalSemaphore : nullptr;

    /* Submit with a fence from the device fence pool. This will be used by the
     * device to determine when the current frame is completed. */
    VulkanCheck(vkQueueSubmit(mQueue,
                              1,
                              &submitInfo,
                              GetVulkanDevice().AllocateFence()));

    /* Need a new command buffer. The old one will be automatically freed at
     * the end of the frame. */
    mCommandBuffer = VK_NULL_HANDLE;

    mWaitSemaphores.clear();
}

void VulkanContext::Wait(const VkSemaphore inSemaphore)
{
    /* Submit any outstanding work. This needs to happen prior to the wait. */
    Submit();

    mWaitSemaphores.emplace_back(inSemaphore);
}

void VulkanContext::Wait(GPUContext& inOtherContext)
{
    Fatal("TODO");
}

void VulkanContext::BeginFrame()
{
    /* Previous GPU usage of this frame's command pools has now completed,
     * reset them. It is OK to reset all threads' command pools from here,
     * because GPUDevice::EndFrame() is not allowed to be called while other
     * threads are recording commands. */
    for (VulkanCommandPool* commandPool : mCommandPools[GetVulkanDevice().GetCurrentFrame()])
    {
        commandPool->Reset();
    }
}

void VulkanContext::EndFrame()
{
    /* Submit outstanding work. */
    if (HaveCommandBuffer())
    {
        Submit();
    }
    else
    {
        /* TODO: Is this something that we'd want to allow? Can't leak the
         * semaphores into the next frame because they're owned by this frame.
         * Perhaps just put them in an empty submission - does that work? */
        AssertMsg(mWaitSemaphores.empty(), "Reached end of frame with outstanding wait semaphores");
    }
}

void VulkanContext::BeginPresent(GPUSwapchain& inSwapchain)
{
    auto& swapchain = static_cast<VulkanSwapchain&>(inSwapchain);

    /* Get a semaphore to be signalled when the swapchain image is available to
     * be rendered to. */
    const VkSemaphore acquireSemaphore = GetVulkanDevice().AllocateSemaphore();

    /* Acquire a swapchain image. */
    swapchain.Acquire(acquireSemaphore);

    /* Subsequent work on the context must wait for the image to have been
     * acquired. */
    Wait(acquireSemaphore);
}

void VulkanContext::EndPresent(GPUSwapchain& inSwapchain)
{
    auto& swapchain = static_cast<VulkanSwapchain&>(inSwapchain);

    /* We need to signal a semaphore after rendering to the swapchain is
     * complete to let the window system know it can present the image. */
    const VkSemaphore completeSemaphore = GetVulkanDevice().AllocateSemaphore();

    /* Submit recorded work to render to the swapchain image. */
    Submit(completeSemaphore);

    /* Present it. */
    swapchain.Present(mQueue, completeSemaphore);
}
