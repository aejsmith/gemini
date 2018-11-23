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

#include "VulkanBuffer.h"
#include "VulkanCommandList.h"
#include "VulkanCommandPool.h"
#include "VulkanDevice.h"
#include "VulkanStagingPool.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"

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

    if (HaveCommandBuffer())
    {
        VulkanCheck(vkEndCommandBuffer(mCommandBuffer));
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

void VulkanContext::ResourceBarrier(const GPUResourceBarrier* const inBarriers,
                                    const size_t                    inCount)
{
    Assert(inCount > 0);

    /* Determine how many barrier structures we're going to need. */
    uint32_t imageBarrierCount  = 0;
    uint32_t bufferBarrierCount = 0;

    for (size_t i = 0; i < inCount; i++)
    {
        Assert(inBarriers[i].resource);
        inBarriers[i].resource->ValidateBarrier(inBarriers[i]);

        if (inBarriers[i].resource->IsTexture())
        {
            imageBarrierCount++;
        }
        else
        {
            bufferBarrierCount++;
        }
    }

    auto imageBarriers  = AllocateZeroedStackArray(VkImageMemoryBarrier, imageBarrierCount);
    auto bufferBarriers = AllocateZeroedStackArray(VkBufferMemoryBarrier, bufferBarrierCount);

    imageBarrierCount  = 0;
    bufferBarrierCount = 0;

    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    for (size_t i = 0; i < inCount; i++)
    {
        VkAccessFlags srcAccessMask  = 0;
        VkAccessFlags dstAccessMask  = 0;
        VkImageLayout oldImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        auto HandleState =
            [&] (const GPUResourceState     inState,
                 const VkPipelineStageFlags inStageMask,
                 const VkAccessFlags        inAccessMask,
                 const VkImageLayout        inLayout = VK_IMAGE_LAYOUT_UNDEFINED)
            {
                if (inBarriers[i].currentState & inState)
                {
                    srcStageMask  |= inStageMask;

                    /* Only write bits are relevant in a source access mask. */
                    srcAccessMask |= inAccessMask & kVkAccessFlagBits_AllWrite;

                    /* Overwrite the image layout. The most preferential layout
                     * is handled last below. The only case where this matters
                     * is depth read-only states + shader read states, where we
                     * want to use the depth/stencil layout as opposed to
                     * SHADER_READ_ONLY. */
                    oldImageLayout = inLayout;
                }

                if (inBarriers[i].newState & inState)
                {
                    dstStageMask  |= inStageMask;
                    dstAccessMask |= inAccessMask;
                    newImageLayout = inLayout;
                }
            };

        HandleState(kGPUResourceState_VertexShaderRead,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        HandleState(kGPUResourceState_FragmentShaderRead,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        HandleState(kGPUResourceState_ComputeShaderRead,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        HandleState(kGPUResourceState_VertexShaderWrite,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL);
        HandleState(kGPUResourceState_FragmentShaderWrite,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL);
        HandleState(kGPUResourceState_ComputeShaderWrite,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL);
        HandleState(kGPUResourceState_VertexShaderUniformRead,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT);
        HandleState(kGPUResourceState_FragmentShaderUniformRead,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT);
        HandleState(kGPUResourceState_ComputeShaderUniformRead,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT);
        HandleState(kGPUResourceState_IndirectBufferRead,
                    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        HandleState(kGPUResourceState_VertexBufferRead,
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        HandleState(kGPUResourceState_IndexBufferRead,
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        HandleState(kGPUResourceState_RenderTarget,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        HandleState(kGPUResourceState_DepthStencilWrite,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        HandleState(kGPUResourceState_DepthReadStencilWrite,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
        HandleState(kGPUResourceState_DepthWriteStencilRead,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);
        HandleState(kGPUResourceState_DepthStencilRead,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        HandleState(kGPUResourceState_TransferRead,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        HandleState(kGPUResourceState_TransferWrite,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        /* Present is a special case in that no synchronisation is required,
         * only a layout transition. Additionally, we discard on transition
         * away from present - we don't need to preserve existing content, and
         * it avoids the problem that on first use the layout will be undefined. */
        if (inBarriers[i].currentState & kGPUResourceState_Present)
        {
            srcStageMask  |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            oldImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        if (inBarriers[i].newState & kGPUResourceState_Present)
        {
            dstStageMask  |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            newImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        /* srcAccessMask can end up 0 e.g. for a read to write transition on a
         * buffer, or on a texture where a layout transition is not necessary.
         * If this happens we don't need a memory barrier, just an execution
         * dependency is sufficient. */
        if (srcAccessMask != 0 || oldImageLayout != newImageLayout)
        {
            if (inBarriers[i].resource->IsTexture())
            {
                auto texture = static_cast<VulkanTexture*>(inBarriers[i].resource);
                auto range   = texture->GetExactSubresourceRange(inBarriers[i].range);

                auto& imageBarrier = imageBarriers[imageBarrierCount++];
                imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageBarrier.srcAccessMask                   = srcAccessMask;
                imageBarrier.dstAccessMask                   = dstAccessMask;
                imageBarrier.oldLayout                       = oldImageLayout;
                imageBarrier.newLayout                       = newImageLayout;
                imageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.image                           = texture->GetHandle();
                imageBarrier.subresourceRange.aspectMask     = texture->GetAspectMask();
                imageBarrier.subresourceRange.baseArrayLayer = range.layerOffset;
                imageBarrier.subresourceRange.layerCount     = range.layerCount;
                imageBarrier.subresourceRange.baseMipLevel   = range.mipOffset;
                imageBarrier.subresourceRange.levelCount     = range.mipCount;
            }
            else
            {
                auto buffer = static_cast<VulkanBuffer*>(inBarriers[i].resource);

                auto& bufferBarrier = bufferBarriers[bufferBarrierCount++];
                bufferBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                bufferBarrier.srcAccessMask       = srcAccessMask;
                bufferBarrier.dstAccessMask       = dstAccessMask;
                bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.buffer              = buffer->GetHandle();
                bufferBarrier.offset              = 0;
                bufferBarrier.size                = buffer->GetSize();
            }
        }
    }

    Assert(srcStageMask != 0);
    Assert(dstStageMask != 0);

    vkCmdPipelineBarrier(GetCommandBuffer(),
                         srcStageMask,
                         dstStageMask,
                         0,
                         0, nullptr,
                         bufferBarrierCount,
                         (bufferBarrierCount > 0) ? bufferBarriers : nullptr,
                         imageBarrierCount,
                         (imageBarrierCount > 0) ? imageBarriers : nullptr);
}

void VulkanContext::ClearTexture(GPUTexture* const          inTexture,
                                 const GPUTextureClearData& inData,
                                 const GPUSubresourceRange& inRange)
{
    auto texture = static_cast<VulkanTexture*>(inTexture);
    auto range   = texture->GetExactSubresourceRange(inRange);

    VkImageSubresourceRange vkRange;
    vkRange.aspectMask     = 0;
    vkRange.baseArrayLayer = range.layerOffset;
    vkRange.layerCount     = range.layerCount;
    vkRange.baseMipLevel   = range.mipOffset;
    vkRange.levelCount     = range.mipCount;

    if (inData.type == GPUTextureClearData::kColour)
    {
        Assert(texture->GetAspectMask() == VK_IMAGE_ASPECT_COLOR_BIT);
        vkRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkClearColorValue value;
        value.float32[0] = inData.colour.r;
        value.float32[1] = inData.colour.g;
        value.float32[2] = inData.colour.b;
        value.float32[3] = inData.colour.a;

        vkCmdClearColorImage(GetCommandBuffer(),
                             texture->GetHandle(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &value,
                             1, &vkRange);
    }
    else
    {
        if (inData.type == GPUTextureClearData::kDepth || inData.type == GPUTextureClearData::kDepthStencil)
        {
            Assert(texture->GetAspectMask() & VK_IMAGE_ASPECT_DEPTH_BIT);
            vkRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        if (inData.type == GPUTextureClearData::kStencil || inData.type == GPUTextureClearData::kDepthStencil)
        {
            Assert(texture->GetAspectMask() & VK_IMAGE_ASPECT_STENCIL_BIT);
            vkRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkClearDepthStencilValue value;
        value.depth   = inData.depth;
        value.stencil = inData.stencil;

        vkCmdClearDepthStencilImage(GetCommandBuffer(),
                                    texture->GetHandle(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &value,
                                    1, &vkRange);
    }
}

void VulkanContext::UploadBuffer(GPUBuffer* const        inDestBuffer,
                                 const GPUStagingBuffer& inSourceBuffer,
                                 const uint32_t          inSize,
                                 const uint32_t          inDestOffset,
                                 const uint32_t          inSourceOffset)
{
    Assert(inSourceBuffer.IsFinalised());
    Assert(inSourceBuffer.GetAccess() == kGPUStagingAccess_Write);

    auto destBuffer       = static_cast<VulkanBuffer*>(inDestBuffer);
    auto sourceAllocation = static_cast<VulkanStagingAllocation*>(inSourceBuffer.GetHandle());

    VkBufferCopy region;
    region.size      = inSize;
    region.dstOffset = inDestOffset;
    region.srcOffset = inSourceOffset;

    vkCmdCopyBuffer(GetCommandBuffer(),
                    sourceAllocation->handle,
                    destBuffer->GetHandle(),
                    1, &region);
}

GPUGraphicsCommandList* VulkanContext::CreateRenderPassImpl(const GPURenderPass& inRenderPass)
{
    // TODO: Command lists should be allocated with a temporary frame allocator
    // that just gets cleared in one go at the end of frame. Need to make sure
    // destruction gets done properly to release the views. Also should be
    // used for storage within the command lists e.g. mCommandBuffers.
    return new VulkanGraphicsCommandList(*this, nullptr, inRenderPass);
}

void VulkanContext::SubmitRenderPassImpl(GPUGraphicsCommandList* const inCmdList)
{
    auto cmdList = static_cast<VulkanGraphicsCommandList*>(inCmdList);
    cmdList->Submit(GetCommandBuffer());
    delete cmdList;
}
