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

#include "Engine/FrameAllocator.h"

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

VulkanContext::VulkanContext(VulkanDevice&  device,
                             const uint8_t  id,
                             const uint32_t queueFamily) :
    GPUGraphicsContext  (device),
    mID                 (id),
    mQueueFamily        (queueFamily),
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

VkCommandBuffer VulkanContext::GetCommandBuffer(const bool allocate)
{
    if (mCommandBuffer == VK_NULL_HANDLE && allocate)
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

void VulkanContext::Submit(const VkSemaphore signalSemaphore)
{
    /* Don't need to do anything if we have nothing to submit and don't have a
     * semaphore to signal. */
    if (!HaveCommandBuffer() && signalSemaphore == VK_NULL_HANDLE)
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
    submitInfo.signalSemaphoreCount = (signalSemaphore != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pSignalSemaphores    = (signalSemaphore != VK_NULL_HANDLE) ? &signalSemaphore : nullptr;

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

void VulkanContext::Wait(const VkSemaphore semaphore)
{
    /* Submit any outstanding work. This needs to happen prior to the wait. */
    Submit();

    mWaitSemaphores.emplace_back(semaphore);
}

void VulkanContext::Wait(GPUContext& otherContext)
{
    ValidateContext();

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

void VulkanContext::BeginPresent(GPUSwapchain& swapchain)
{
    ValidateContext();

    auto& vkSwapchain = static_cast<VulkanSwapchain&>(swapchain);

    /* Get a semaphore to be signalled when the swapchain image is available to
     * be rendered to. */
    const VkSemaphore acquireSemaphore = GetVulkanDevice().AllocateSemaphore();

    /* Acquire a swapchain image. */
    vkSwapchain.Acquire(acquireSemaphore);

    /* Subsequent work on the context must wait for the image to have been
     * acquired. */
    Wait(acquireSemaphore);
}

void VulkanContext::EndPresent(GPUSwapchain& swapchain)
{
    ValidateContext();

    auto& vkSwapchain = static_cast<VulkanSwapchain&>(swapchain);

    /* We need to signal a semaphore after rendering to the swapchain is
     * complete to let the window system know it can present the image. */
    const VkSemaphore completeSemaphore = GetVulkanDevice().AllocateSemaphore();

    /* Submit recorded work to render to the swapchain image. */
    Submit(completeSemaphore);

    /* Present it. */
    vkSwapchain.Present(mQueue, completeSemaphore);
}

void VulkanContext::ResourceBarrier(const GPUResourceBarrier* const barriers,
                                    const size_t                    count)
{
    ValidateContext();

    Assert(count > 0);

    /* Determine how many barrier structures we're going to need. */
    uint32_t imageBarrierCount  = 0;
    uint32_t bufferBarrierCount = 0;

    for (size_t i = 0; i < count; i++)
    {
        Assert(barriers[i].resource);
        barriers[i].resource->ValidateBarrier(barriers[i]);

        if (barriers[i].resource->IsTexture())
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

    for (size_t i = 0; i < count; i++)
    {
        const bool isTexture = barriers[i].resource->IsTexture();

        VkAccessFlags srcAccessMask  = 0;
        VkAccessFlags dstAccessMask  = 0;
        VkImageLayout oldImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        auto HandleState = [&] (const GPUResourceState     state,
                                const VkPipelineStageFlags stageMask,
                                const VkAccessFlags        accessMask,
                                const VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED)
        {
            if (barriers[i].currentState & state)
            {
                srcStageMask  |= stageMask;

                /* Only write bits are relevant in a source access mask. */
                srcAccessMask |= accessMask & kVkAccessFlagBits_AllWrite;

                /* Overwrite the image layout. The most preferential layout is
                 * handled last below. The only case where this matters is depth
                 * read-only states + shader read states, where we want to use
                 * the depth/stencil layout as opposed to
                 * SHADER_READ_ONLY. */
                oldImageLayout = layout;
            }

            if (barriers[i].newState & state)
            {
                dstStageMask  |= stageMask;
                dstAccessMask |= accessMask;
                newImageLayout = layout;
            }
        };

        HandleState(kGPUResourceState_VertexShaderRead,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        HandleState(kGPUResourceState_PixelShaderRead,
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
        HandleState(kGPUResourceState_PixelShaderWrite,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL);
        HandleState(kGPUResourceState_ComputeShaderWrite,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_GENERAL);
        HandleState(kGPUResourceState_VertexShaderConstantRead,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT);
        HandleState(kGPUResourceState_PixelShaderConstantRead,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT);
        HandleState(kGPUResourceState_ComputeShaderConstantRead,
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
         * away from present the first time this is done for this swapchain
         * image within this frame - we don't need to preserve existing content,
         * and it avoids the problem that on first use the layout will be
         * undefined. */
        if (barriers[i].currentState & kGPUResourceState_Present)
        {
            srcStageMask  |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            Assert(isTexture);
            auto texture = static_cast<VulkanTexture*>(barriers[i].resource);
            Assert(texture->IsSwapchain());

            if (texture->GetAndResetNeedDiscard())
            {
                oldImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            else
            {
                oldImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
        }

        if (barriers[i].newState & kGPUResourceState_Present)
        {
            dstStageMask  |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            newImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        if (barriers[i].discard)
        {
            /* Undefined can always be specified as the old layout which
             * indicates we don't care about current content. */
            oldImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        /* srcAccessMask can end up 0 e.g. for a read to write transition on a
         * buffer, or on a texture where a layout transition is not necessary.
         * If this happens we don't need a memory barrier, just an execution
         * dependency is sufficient. */
        if (srcAccessMask != 0 || (isTexture && oldImageLayout != newImageLayout))
        {
            if (isTexture)
            {
                auto texture = static_cast<VulkanTexture*>(barriers[i].resource);
                auto range   = texture->GetExactSubresourceRange(barriers[i].range);

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
                auto buffer = static_cast<VulkanBuffer*>(barriers[i].resource);

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

    Assert(dstStageMask != 0);

    if (srcStageMask != 0 || bufferBarrierCount > 0 || imageBarrierCount > 0)
    {
        /* Can happen for an initial transition from kGPUResourceState_None. */
        if (srcStageMask == 0)
        {
            srcStageMask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

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
}

void VulkanContext::BlitTexture(GPUTexture* const    destTexture,
                                const GPUSubresource destSubresource,
                                const glm::ivec3&    destOffset,
                                const glm::ivec3&    destSize,
                                GPUTexture* const    sourceTexture,
                                const GPUSubresource sourceSubresource,
                                const glm::ivec3&    sourceOffset,
                                const glm::ivec3&    sourceSize)
{
    ValidateContext();

    auto vkDestTexture   = static_cast<VulkanTexture*>(destTexture);
    auto vkSourceTexture = static_cast<VulkanTexture*>(sourceTexture);

    VkImageBlit imageBlit;
    imageBlit.srcSubresource.aspectMask     = vkSourceTexture->GetAspectMask();
    imageBlit.srcSubresource.baseArrayLayer = sourceSubresource.layer;
    imageBlit.srcSubresource.layerCount     = 1;
    imageBlit.srcSubresource.mipLevel       = sourceSubresource.mipLevel;
    imageBlit.srcOffsets[0].x               = sourceOffset.x;
    imageBlit.srcOffsets[0].y               = sourceOffset.y;
    imageBlit.srcOffsets[0].z               = sourceOffset.z;
    imageBlit.srcOffsets[1].x               = sourceOffset.x + sourceSize.x;
    imageBlit.srcOffsets[1].y               = sourceOffset.y + sourceSize.y;
    imageBlit.srcOffsets[1].z               = sourceOffset.z + sourceSize.z;
    imageBlit.dstSubresource.aspectMask     = vkDestTexture->GetAspectMask();
    imageBlit.dstSubresource.baseArrayLayer = destSubresource.layer;
    imageBlit.dstSubresource.layerCount     = 1;
    imageBlit.dstSubresource.mipLevel       = destSubresource.mipLevel;
    imageBlit.dstOffsets[0].x               = destOffset.x;
    imageBlit.dstOffsets[0].y               = destOffset.y;
    imageBlit.dstOffsets[0].z               = destOffset.z;
    imageBlit.dstOffsets[1].x               = destOffset.x + destSize.x;
    imageBlit.dstOffsets[1].y               = destOffset.y + destSize.y;
    imageBlit.dstOffsets[1].z               = destOffset.z + destSize.z;

    vkCmdBlitImage(GetCommandBuffer(),
                   vkSourceTexture->GetHandle(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   vkDestTexture->GetHandle(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &imageBlit,
                   VK_FILTER_LINEAR);
}

void VulkanContext::ClearTexture(GPUTexture* const          texture,
                                 const GPUTextureClearData& data,
                                 const GPUSubresourceRange& range)
{
    ValidateContext();

    auto vkTexture  = static_cast<VulkanTexture*>(texture);
    auto exactRange = vkTexture->GetExactSubresourceRange(range);

    VkImageSubresourceRange vkRange;
    vkRange.aspectMask     = 0;
    vkRange.baseArrayLayer = exactRange.layerOffset;
    vkRange.layerCount     = exactRange.layerCount;
    vkRange.baseMipLevel   = exactRange.mipOffset;
    vkRange.levelCount     = exactRange.mipCount;

    if (data.type == GPUTextureClearData::kColour)
    {
        Assert(vkTexture->GetAspectMask() == VK_IMAGE_ASPECT_COLOR_BIT);
        vkRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkClearColorValue value;
        value.float32[0] = data.colour.r;
        value.float32[1] = data.colour.g;
        value.float32[2] = data.colour.b;
        value.float32[3] = data.colour.a;

        vkCmdClearColorImage(GetCommandBuffer(),
                             vkTexture->GetHandle(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &value,
                             1, &vkRange);
    }
    else
    {
        if (data.type == GPUTextureClearData::kDepth || data.type == GPUTextureClearData::kDepthStencil)
        {
            Assert(vkTexture->GetAspectMask() & VK_IMAGE_ASPECT_DEPTH_BIT);
            vkRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        if (data.type == GPUTextureClearData::kStencil || data.type == GPUTextureClearData::kDepthStencil)
        {
            Assert(vkTexture->GetAspectMask() & VK_IMAGE_ASPECT_STENCIL_BIT);
            vkRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkClearDepthStencilValue value;
        value.depth   = data.depth;
        value.stencil = data.stencil;

        vkCmdClearDepthStencilImage(GetCommandBuffer(),
                                    vkTexture->GetHandle(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &value,
                                    1, &vkRange);
    }
}

void VulkanContext::UploadBuffer(GPUBuffer* const        destBuffer,
                                 const GPUStagingBuffer& sourceBuffer,
                                 const uint32_t          size,
                                 const uint32_t          destOffset,
                                 const uint32_t          sourceOffset)
{
    ValidateContext();

    Assert(sourceBuffer.IsFinalised());
    Assert(sourceBuffer.GetAccess() == kGPUStagingAccess_Write);

    auto vkDestBuffer     = static_cast<VulkanBuffer*>(destBuffer);
    auto sourceAllocation = static_cast<VulkanStagingAllocation*>(sourceBuffer.GetHandle());

    VkBufferCopy region;
    region.size      = size;
    region.dstOffset = destOffset;
    region.srcOffset = sourceOffset;

    vkCmdCopyBuffer(GetCommandBuffer(),
                    sourceAllocation->handle,
                    vkDestBuffer->GetHandle(),
                    1, &region);
}

void VulkanContext::UploadTexture(GPUTexture* const        destTexture,
                                  const GPUStagingTexture& sourceTexture)
{
    ValidateContext();

    Assert(sourceTexture.IsFinalised());
    Assert(sourceTexture.GetAccess() == kGPUStagingAccess_Write);
    Assert(destTexture->SizeMatches(sourceTexture));
    Assert(destTexture->GetFormat() == sourceTexture.GetFormat());
    Assert(PixelFormatInfo::IsColour(destTexture->GetFormat()));

    auto vkDestTexture    = static_cast<VulkanTexture*>(destTexture);
    auto sourceAllocation = static_cast<VulkanStagingAllocation*>(sourceTexture.GetHandle());

    const auto regions = AllocateStackArray(VkBufferImageCopy, destTexture->GetArraySize() * destTexture->GetNumMipLevels());

    uint32_t subresourceIndex = 0;
    for (uint32_t layer = 0; layer < vkDestTexture->GetArraySize(); layer++)
    {
        for (uint32_t mipLevel = 0; mipLevel < vkDestTexture->GetNumMipLevels(); mipLevel++)
        {
            auto& region = regions[subresourceIndex++];

            const uint32_t width  = vkDestTexture->GetMipWidth(mipLevel);
            const uint32_t height = vkDestTexture->GetMipHeight(mipLevel);
            const uint32_t depth  = vkDestTexture->GetMipDepth(mipLevel);

            region.bufferOffset                    = sourceTexture.GetSubresourceOffset({mipLevel, layer});
            region.bufferRowLength                 = 0;
            region.bufferImageHeight               = 0;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = layer;
            region.imageSubresource.layerCount     = 1;
            region.imageSubresource.mipLevel       = mipLevel;
            region.imageOffset                     = { 0, 0, 0 };
            region.imageExtent                     = { width, height, depth };
        }
    }

    vkCmdCopyBufferToImage(GetCommandBuffer(),
                           sourceAllocation->handle,
                           vkDestTexture->GetHandle(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           subresourceIndex,
                           regions);
}

void VulkanContext::UploadTexture(GPUTexture* const        destTexture,
                                  const GPUSubresource     destSubresource,
                                  const glm::ivec3&        destOffset,
                                  const GPUStagingTexture& sourceTexture,
                                  const GPUSubresource     sourceSubresource,
                                  const glm::ivec3&        sourceOffset,
                                  const glm::ivec3&        size)
{
    ValidateContext();

    Assert(sourceTexture.IsFinalised());
    Assert(sourceTexture.GetAccess() == kGPUStagingAccess_Write);
    Assert(destTexture->GetFormat() == sourceTexture.GetFormat());
    Assert(PixelFormatInfo::IsColour(destTexture->GetFormat()));

    auto vkDestTexture    = static_cast<VulkanTexture*>(destTexture);
    auto sourceAllocation = static_cast<VulkanStagingAllocation*>(sourceTexture.GetHandle());

    const uint32_t destWidth  = vkDestTexture->GetMipWidth(destSubresource.mipLevel);  Unused(destWidth);
    const uint32_t destHeight = vkDestTexture->GetMipHeight(destSubresource.mipLevel); Unused(destHeight);
    const uint32_t destDepth  = vkDestTexture->GetMipDepth(destSubresource.mipLevel);  Unused(destDepth);

    Assert(static_cast<uint32_t>(destOffset.x + size.x) <= destWidth);
    Assert(static_cast<uint32_t>(destOffset.y + size.y) <= destHeight);
    Assert(static_cast<uint32_t>(destOffset.z + size.z) <= destDepth);

    const uint32_t sourceWidth  = sourceTexture.GetMipWidth(sourceSubresource.mipLevel);  Unused(sourceWidth);
    const uint32_t sourceHeight = sourceTexture.GetMipHeight(sourceSubresource.mipLevel); Unused(sourceHeight);
    const uint32_t sourceDepth  = sourceTexture.GetMipDepth(sourceSubresource.mipLevel);  Unused(sourceDepth);

    Assert(static_cast<uint32_t>(sourceOffset.x + size.x) <= sourceWidth);
    Assert(static_cast<uint32_t>(sourceOffset.y + size.y) <= sourceHeight);
    Assert(static_cast<uint32_t>(sourceOffset.z + size.z) <= sourceDepth);

    const size_t bytesPerPixel = PixelFormatInfo::BytesPerPixel(sourceTexture.GetFormat());
    const size_t bytesPerRow   = bytesPerPixel * sourceWidth;
    const size_t bytesPerSlice = bytesPerRow * sourceHeight;

    VkBufferImageCopy region;
    region.bufferOffset                    = sourceTexture.GetSubresourceOffset(sourceSubresource) +
                                             (sourceOffset.z * bytesPerSlice) +
                                             (sourceOffset.y * bytesPerRow) +
                                             (sourceOffset.x * bytesPerPixel);
    region.bufferRowLength                 = sourceWidth;
    region.bufferImageHeight               = sourceHeight;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = destSubresource.layer;
    region.imageSubresource.layerCount     = 1;
    region.imageSubresource.mipLevel       = destSubresource.mipLevel;
    region.imageOffset.x                   = destOffset.x;
    region.imageOffset.y                   = destOffset.y;
    region.imageOffset.z                   = destOffset.z;
    region.imageExtent.width               = size.x;
    region.imageExtent.height              = size.y;
    region.imageExtent.depth               = size.z;

    vkCmdCopyBufferToImage(GetCommandBuffer(),
                           sourceAllocation->handle,
                           vkDestTexture->GetHandle(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);
}

GPUComputeCommandList* VulkanContext::CreateComputePassImpl()
{
    return FrameAllocator::New<VulkanComputeCommandList>(*this, nullptr);
}

void VulkanContext::SubmitComputePassImpl(GPUComputeCommandList* const cmdList)
{
    auto vkCmdList = static_cast<VulkanComputeCommandList*>(cmdList);
    vkCmdList->Submit(GetCommandBuffer());
    FrameAllocator::Delete(vkCmdList);
}

GPUGraphicsCommandList* VulkanContext::CreateRenderPassImpl(const GPURenderPass& renderPass)
{
    return FrameAllocator::New<VulkanGraphicsCommandList>(*this, nullptr, renderPass);
}

void VulkanContext::SubmitRenderPassImpl(GPUGraphicsCommandList* const cmdList)
{
    auto vkCmdList = static_cast<VulkanGraphicsCommandList*>(cmdList);
    vkCmdList->Submit(GetCommandBuffer());
    FrameAllocator::Delete(vkCmdList);
}

#if GEMINI_GPU_MARKERS

void VulkanContext::BeginMarker(const char* const label)
{
    if (GetVulkanDevice().HasCap(VulkanDevice::kCap_DebugMarker))
    {
        VkDebugMarkerMarkerInfoEXT markerInfo = {};
        markerInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        markerInfo.pMarkerName = label;

        vkCmdDebugMarkerBeginEXT(GetCommandBuffer(), &markerInfo);
    }
}

void VulkanContext::EndMarker()
{
    if (GetVulkanDevice().HasCap(VulkanDevice::kCap_DebugMarker))
    {
        vkCmdDebugMarkerEndEXT(GetCommandBuffer());
    }
}

#endif
