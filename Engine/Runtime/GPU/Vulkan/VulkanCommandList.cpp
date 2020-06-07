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

#include "VulkanCommandList.h"

#include "VulkanArgumentSet.h"
#include "VulkanBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanPipeline.h"
#include "VulkanTransientPool.h"

template <typename T>
inline VulkanCommandList<T>::VulkanCommandList() :
    mCommandBuffer  (VK_NULL_HANDLE),
    mDescriptorSets {}
{
}

template <typename T>
inline VulkanContext& VulkanCommandList<T>::GetVulkanContext() const
{
    return static_cast<VulkanContext&>(GetT().GetContext());
}

template <typename T>
inline VkCommandBuffer VulkanCommandList<T>::GetCommandBuffer()
{
    GetT().ValidateCommand();

    if (Unlikely(mCommandBuffer == VK_NULL_HANDLE))
    {
        mCommandBuffer = GetVulkanContext().GetCommandPool().AllocateSecondary();

        GetT().BeginCommandBuffer(mCommandBuffer);
    }

    return mCommandBuffer;
}

template <typename T>
inline void VulkanCommandList<T>::SubmitImpl(const VkCommandBuffer buffer) const
{
    if (mCommandBuffers.size() > 0)
    {
        vkCmdExecuteCommands(buffer, mCommandBuffers.size(), mCommandBuffers.data());
    }
}

template <typename T>
inline void VulkanCommandList<T>::EndImpl()
{
    if (mCommandBuffer != VK_NULL_HANDLE)
    {
        VulkanCheck(vkEndCommandBuffer(mCommandBuffer));

        mCommandBuffers.emplace_back(mCommandBuffer);
        mCommandBuffer = VK_NULL_HANDLE;
    }
}

template <typename T>
inline void VulkanCommandList<T>::SubmitChildrenImpl(GPUCommandList** const children,
                                                     const size_t           count)
{
    /* The submitted children should be ordered after any previous commands on
     * this command list. End the current command buffer, if any. */
    EndImpl();

    for (size_t i = 0; i < count; i++)
    {
        const auto cmdList = static_cast<const T*>(children[i]);

        mCommandBuffers.insert(mCommandBuffers.end(),
                               cmdList->mCommandBuffers.begin(),
                               cmdList->mCommandBuffers.end());

        FrameAllocator::Delete(cmdList);
    }
}

template <typename T>
inline void VulkanCommandList<T>::SetArgumentsImpl(const uint8_t         index,
                                                   GPUArgumentSet* const set)
{
    const auto vkSet = static_cast<VulkanArgumentSet*>(set);

    if (vkSet->GetHandle() != mDescriptorSets[index])
    {
        auto& argumentState = GetT().mArgumentState[index];
        argumentState.dirty = true;

        mDescriptorSets[index] = vkSet->GetHandle();
    }
}

template <typename T>
inline void VulkanCommandList<T>::SetArgumentsImpl(const uint8_t            index,
                                                   const GPUArgument* const arguments)
{
    auto& argumentState = GetT().mArgumentState[index];
    argumentState.dirty = true;

    const auto layout = static_cast<const VulkanArgumentSetLayout*>(argumentState.layout);

    if (layout->IsConstantOnly())
    {
        mDescriptorSets[index] = layout->GetConstantOnlySet();
    }
    else
    {
        mDescriptorSets[index] = GetVulkanContext().GetCommandPool().AllocateDescriptorSet(layout->GetHandle());

        VulkanArgumentSet::Write(mDescriptorSets[index], layout, arguments);
    }
}

template <typename T>
inline void VulkanCommandList<T>::BindDescriptorSets(const VkPipelineBindPoint bindPoint,
                                                     const VkPipelineLayout    pipelineLayout)
{
    GetT().ValidateArguments();

    /* Try to group multiple contiguous set changes into a single call. */
    VkDescriptorSet sets[kMaxArgumentSets];
    uint32_t firstSet = 0;
    uint32_t setCount = 0;

    uint32_t dynamicOffsets[kMaxArgumentSets * kMaxArgumentsPerSet];
    uint32_t dynamicOffsetCount = 0;

    auto DoBind =
        [&] ()
        {
            vkCmdBindDescriptorSets(GetCommandBuffer(),
                                    bindPoint,
                                    pipelineLayout,
                                    firstSet,
                                    setCount,
                                    sets,
                                    dynamicOffsetCount,
                                    dynamicOffsets);

            setCount           = 0;
            dynamicOffsetCount = 0;
        };

    for (uint32_t setIndex = 0; setIndex < kMaxArgumentSets; setIndex++)
    {
        auto& argumentState = GetT().mArgumentState[setIndex];

        if (argumentState.dirty && argumentState.layout)
        {
            if (setCount > 0 && firstSet + setCount != setIndex)
            {
                DoBind();
            }

            if (setCount == 0)
            {
                firstSet = setIndex;
            }

            sets[setCount++] = mDescriptorSets[setIndex];

            for (size_t argumentIndex = 0; argumentIndex < argumentState.layout->GetArgumentCount(); argumentIndex++)
            {
                if (argumentState.layout->GetArguments()[argumentIndex] == kGPUArgumentType_Constants)
                {
                    dynamicOffsets[dynamicOffsetCount++] = argumentState.constants[argumentIndex];
                }
            }

            argumentState.dirty = false;
        }
    }

    if (setCount > 0)
    {
        DoBind();
    }
}

VulkanComputeCommandList::VulkanComputeCommandList(VulkanContext&                     context,
                                                   const GPUComputeCommandList* const parent) :
    GPUComputeCommandList   (context, parent)
{
}

void VulkanComputeCommandList::BeginCommandBuffer(const VkCommandBuffer buffer) const
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = &inheritanceInfo;

    VulkanCheck(vkBeginCommandBuffer(buffer, &beginInfo));
}

void VulkanComputeCommandList::Submit(const VkCommandBuffer buffer) const
{
    SubmitImpl(buffer);
}

GPUCommandList* VulkanComputeCommandList::CreateChildImpl()
{
    return FrameAllocator::New<VulkanComputeCommandList>(GetVulkanContext(), this);
}

void VulkanComputeCommandList::PreDispatch()
{
    const auto pipeline = static_cast<VulkanComputePipeline*>(mPipeline);

    if (mPipelineDirty)
    {
        vkCmdBindPipeline(GetCommandBuffer(),
                          VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline->GetHandle());

        mPipelineDirty = false;
    }

    BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout());
}

void VulkanComputeCommandList::Dispatch(const uint32_t groupCountX,
                                        const uint32_t groupCountY,
                                        const uint32_t groupCountZ)
{
    PreDispatch();

    vkCmdDispatch(GetCommandBuffer(),
                  groupCountX,
                  groupCountY,
                  groupCountZ);
}

VulkanGraphicsCommandList::VulkanGraphicsCommandList(VulkanContext&                      context,
                                                     const GPUGraphicsCommandList* const parent,
                                                     const GPURenderPass&                renderPass) :
    GPUGraphicsCommandList (context, parent, renderPass)
{
    if (mParent)
    {
        /* Inherit Vulkan objects from the parent. */
        const auto vkParent = static_cast<const VulkanGraphicsCommandList*>(parent);
        mVulkanRenderPass   = vkParent->GetVulkanRenderPass();
        mFramebuffer        = vkParent->GetFramebuffer();
    }
    else
    {
        /* Get new ones from the device. */
        GetVulkanDevice().GetRenderPass(mRenderPass,
                                        mVulkanRenderPass,
                                        mFramebuffer);
    }
}

void VulkanGraphicsCommandList::BeginCommandBuffer(const VkCommandBuffer buffer) const
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass  = mVulkanRenderPass;
    inheritanceInfo.subpass     = 0;
    inheritanceInfo.framebuffer = mFramebuffer;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                                 VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = &inheritanceInfo;

    VulkanCheck(vkBeginCommandBuffer(buffer, &beginInfo));
}

void VulkanGraphicsCommandList::Submit(const VkCommandBuffer buffer) const
{
    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass  = mVulkanRenderPass;
    beginInfo.framebuffer = mFramebuffer;

    uint32_t layers;

    mRenderPass.GetDimensions(beginInfo.renderArea.extent.width,
                              beginInfo.renderArea.extent.height,
                              layers);

    /* Indices in this array must match up with the attachments array in the
     * Vulkan render pass. */
    VkClearValue clearValues[kMaxRenderPassColourAttachments + 1];
    uint32_t attachmentIndex = 0;

    for (size_t i = 0; i < ArraySize(mRenderPass.colour); i++)
    {
        const auto& attachment = mRenderPass.colour[i];

        if (attachment.view)
        {
            if (attachment.loadOp == kGPULoadOp_Clear)
            {
                auto& clearValue = clearValues[attachmentIndex];
                clearValue.color.float32[0] = attachment.clearValue.colour.r;
                clearValue.color.float32[1] = attachment.clearValue.colour.g;
                clearValue.color.float32[2] = attachment.clearValue.colour.b;
                clearValue.color.float32[3] = attachment.clearValue.colour.a;

                beginInfo.clearValueCount = attachmentIndex + 1;
            }

            attachmentIndex++;
        }
    }

    const auto& attachment = mRenderPass.depthStencil;

    if (attachment.view)
    {
        if (attachment.loadOp == kGPULoadOp_Clear || attachment.stencilLoadOp == kGPULoadOp_Clear)
        {
            auto& clearValue = clearValues[attachmentIndex];
            clearValue.depthStencil.depth   = attachment.clearValue.depth;
            clearValue.depthStencil.stencil = attachment.clearValue.stencil;

            beginInfo.clearValueCount = attachmentIndex + 1;
        }

        attachmentIndex++;
    }

    beginInfo.pClearValues = (beginInfo.clearValueCount > 0) ? clearValues : nullptr;

    vkCmdBeginRenderPass(buffer,
                         &beginInfo,
                         VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    SubmitImpl(buffer);

    vkCmdEndRenderPass(buffer);
}

GPUCommandList* VulkanGraphicsCommandList::CreateChildImpl()
{
    return FrameAllocator::New<VulkanGraphicsCommandList>(GetVulkanContext(), this, mRenderPass);
}

void VulkanGraphicsCommandList::PreDraw(const bool isIndexed)
{
    const auto pipeline = static_cast<const VulkanPipeline*>(mPipeline);

    if (mDirtyState & kDirtyState_Pipeline)
    {
        vkCmdBindPipeline(GetCommandBuffer(),
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->GetHandle());

        if (pipeline->NeedsDummyVertexBuffer())
        {
            const uint32_t value = 0;
            WriteVertexBuffer(pipeline->GetDummyVertexBuffer(), &value, sizeof(value));
        }

        mDirtyState &= ~kDirtyState_Pipeline;
    }

    BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout());

    if (mDirtyState & kDirtyState_Viewport)
    {
        VkViewport viewport;

        /* Use a negative height to have Y pointing up. This means we need to
         * specify Y as pointing to the lower left of the viewport. */
        viewport.x        = mViewport.rect.x;
        viewport.y        = mViewport.rect.y + mViewport.rect.height;
        viewport.width    = mViewport.rect.width;
        viewport.height   = -mViewport.rect.height;
        viewport.minDepth = mViewport.minDepth;
        viewport.maxDepth = mViewport.maxDepth;

        vkCmdSetViewport(GetCommandBuffer(),
                         0,
                         1,
                         &viewport);

        mDirtyState &= ~kDirtyState_Viewport;
    }

    if (mDirtyState & kDirtyState_Scissor)
    {
        VkRect2D scissor;
        scissor.offset.x      = mScissor.x;
        scissor.offset.y      = mScissor.y;
        scissor.extent.width  = mScissor.width;
        scissor.extent.height = mScissor.height;

        vkCmdSetScissor(GetCommandBuffer(),
                        0,
                        1,
                        &scissor);

        mDirtyState &= ~kDirtyState_Scissor;
    }

    const size_t vertexDirtyStart = mDirtyVertexBuffers.FindFirst();
    const size_t vertexDirtyEnd   = mDirtyVertexBuffers.FindLast();

    if (vertexDirtyStart != kMaxVertexAttributes)
    {
        VkBuffer handles[kMaxVertexAttributes];
        VkDeviceSize offsets[kMaxVertexAttributes];

        const size_t count = (vertexDirtyEnd - vertexDirtyStart) + 1;

        for (size_t i = 0; i < count; i++)
        {
            auto& vertexBuffer = mVertexBuffers[i];

            handles[i] =
                (vertexBuffer.buffer)
                    ? static_cast<VulkanBuffer*>(vertexBuffer.buffer)->GetHandle()
                    : GetVulkanDevice().GetGeometryPool().GetHandle();

            offsets[i] =
                (vertexBuffer.offset != kInvalidBuffer)
                    ? vertexBuffer.offset
                    : 0;
        }

        vkCmdBindVertexBuffers(GetCommandBuffer(),
                               vertexDirtyStart,
                               count,
                               handles,
                               offsets);

        mDirtyVertexBuffers.Reset();
    }

    if (isIndexed && mDirtyState & kDirtyState_IndexBuffer)
    {
        Assert(mIndexBuffer.offset != kInvalidBuffer);

        const VkBuffer handle =
            (mIndexBuffer.buffer)
                ? static_cast<VulkanBuffer*>(mIndexBuffer.buffer)->GetHandle()
                : GetVulkanDevice().GetGeometryPool().GetHandle();

        const VkIndexType indexType =
            (mIndexBuffer.type == kGPUIndexType_32)
                ? VK_INDEX_TYPE_UINT32
                : VK_INDEX_TYPE_UINT16;

        vkCmdBindIndexBuffer(GetCommandBuffer(),
                             handle,
                             mIndexBuffer.offset,
                             indexType);

        mDirtyState &= ~kDirtyState_IndexBuffer;
    }
}

void VulkanGraphicsCommandList::Draw(const uint32_t vertexCount,
                                     const uint32_t firstVertex)
{
    PreDraw(false);

    vkCmdDraw(GetCommandBuffer(),
              vertexCount,
              1,
              firstVertex,
              0);
}

void VulkanGraphicsCommandList::DrawIndexed(const uint32_t indexCount,
                                            const uint32_t firstIndex,
                                            const int32_t  vertexOffset)
{
    PreDraw(true);

    vkCmdDrawIndexed(GetCommandBuffer(),
                     indexCount,
                     1,
                     firstIndex,
                     vertexOffset,
                     0);
}

uint32_t VulkanGraphicsCommandList::AllocateTransientBuffer(const size_t size,
                                                            void*&       outMapping)
{
    return GetVulkanDevice().GetGeometryPool().Allocate(size, outMapping);
}
