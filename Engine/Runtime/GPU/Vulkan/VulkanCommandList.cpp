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

#include "VulkanCommandList.h"

#include "VulkanArgumentSet.h"
#include "VulkanBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanPipeline.h"

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
inline void VulkanCommandList<T>::SubmitImpl(const VkCommandBuffer inBuffer) const
{
    if (mCommandBuffers.size() > 0)
    {
        vkCmdExecuteCommands(inBuffer, mCommandBuffers.size(), mCommandBuffers.data());
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
inline void VulkanCommandList<T>::SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                     const size_t           inCount)
{
    /* The submitted children should be ordered after any previous commands on
     * this command list. End the current command buffer, if any. */
    EndImpl();

    for (size_t i = 0; i < inCount; i++)
    {
        const auto cmdList = static_cast<const T*>(inChildren[i]);

        mCommandBuffers.insert(mCommandBuffers.end(),
                               cmdList->mCommandBuffers.begin(),
                               cmdList->mCommandBuffers.end());

        delete cmdList;
    }
}

template <typename T>
inline void VulkanCommandList<T>::SetArgumentsImpl(const uint8_t         inIndex,
                                                   GPUArgumentSet* const inSet)
{
    const auto set = static_cast<VulkanArgumentSet*>(inSet);

    if (set->GetHandle() != mDescriptorSets[inIndex])
    {
        auto& argumentState = GetT().mArgumentState[inIndex];
        argumentState.dirty = true;

        mDescriptorSets[inIndex] = set->GetHandle();
    }
}

template <typename T>
inline void VulkanCommandList<T>::SetArgumentsImpl(const uint8_t            inIndex,
                                                   const GPUArgument* const inArguments)
{
    auto& argumentState = GetT().mArgumentState[inIndex];
    argumentState.dirty = true;

    const auto layout = static_cast<const VulkanArgumentSetLayout*>(argumentState.layout);

    if (layout->IsConstantOnly())
    {
        mDescriptorSets[inIndex] = layout->GetConstantOnlySet();
    }
    else
    {
        mDescriptorSets[inIndex] = GetVulkanContext().GetCommandPool().AllocateDescriptorSet(layout->GetHandle());

        VulkanArgumentSet::Write(mDescriptorSets[inIndex], layout, inArguments);
    }
}

template <typename T>
inline void VulkanCommandList<T>::BindDescriptorSets(const VkPipelineBindPoint inBindPoint,
                                                     const VkPipelineLayout    inPipelineLayout)
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
                                    inBindPoint,
                                    inPipelineLayout,
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

VulkanComputeCommandList::VulkanComputeCommandList(VulkanContext&                     inContext,
                                                   const GPUComputeCommandList* const inParent) :
    GPUComputeCommandList   (inContext, inParent)
{
}

void VulkanComputeCommandList::BeginCommandBuffer(const VkCommandBuffer inBuffer) const
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = &inheritanceInfo;

    VulkanCheck(vkBeginCommandBuffer(inBuffer, &beginInfo));
}

void VulkanComputeCommandList::Submit(const VkCommandBuffer inBuffer) const
{
    SubmitImpl(inBuffer);
}

GPUCommandList* VulkanComputeCommandList::CreateChildImpl()
{
    return new VulkanComputeCommandList(GetVulkanContext(), this);
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

void VulkanComputeCommandList::Dispatch(const uint32_t inGroupCountX,
                                        const uint32_t inGroupCountY,
                                        const uint32_t inGroupCountZ)
{
    PreDispatch();

    vkCmdDispatch(GetCommandBuffer(),
                  inGroupCountX,
                  inGroupCountY,
                  inGroupCountZ);
}

VulkanGraphicsCommandList::VulkanGraphicsCommandList(VulkanContext&                      inContext,
                                                     const GPUGraphicsCommandList* const inParent,
                                                     const GPURenderPass&                inRenderPass) :
    GPUGraphicsCommandList (inContext, inParent, inRenderPass)
{
    if (mParent)
    {
        /* Inherit Vulkan objects from the parent. */
        const auto parent = static_cast<const VulkanGraphicsCommandList*>(inParent);
        mVulkanRenderPass = parent->GetVulkanRenderPass();
        mFramebuffer      = parent->GetFramebuffer();
    }
    else
    {
        /* Get new ones from the device. */
        GetVulkanDevice().GetRenderPass(mRenderPass,
                                        mVulkanRenderPass,
                                        mFramebuffer);
    }
}

void VulkanGraphicsCommandList::BeginCommandBuffer(const VkCommandBuffer inBuffer) const
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

    VulkanCheck(vkBeginCommandBuffer(inBuffer, &beginInfo));
}

void VulkanGraphicsCommandList::Submit(const VkCommandBuffer inBuffer) const
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

    vkCmdBeginRenderPass(inBuffer,
                         &beginInfo,
                         VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    SubmitImpl(inBuffer);

    vkCmdEndRenderPass(inBuffer);
}

GPUCommandList* VulkanGraphicsCommandList::CreateChildImpl()
{
    return new VulkanGraphicsCommandList(GetVulkanContext(), this, mRenderPass);
}

void VulkanGraphicsCommandList::PreDraw(const bool inIsIndexed)
{
    const auto pipeline = static_cast<VulkanPipeline*>(mPipeline);

    if (mDirtyState & kDirtyState_Pipeline)
    {
        vkCmdBindPipeline(GetCommandBuffer(),
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->GetHandle());

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

    // TODO: Could batch these.
    for (size_t i = 0; i < kMaxVertexAttributes; i++)
    {
        auto& vertexBuffer = mVertexBuffers[i];

        if (vertexBuffer.dirty && vertexBuffer.buffer)
        {
            const auto buffer         = static_cast<VulkanBuffer*>(vertexBuffer.buffer);
            const VkBuffer handle     = buffer->GetHandle();
            const VkDeviceSize offset = vertexBuffer.offset;

            vkCmdBindVertexBuffers(GetCommandBuffer(),
                                   i,
                                   1,
                                   &handle,
                                   &offset);

            vertexBuffer.dirty = false;
        }
    }

    if (inIsIndexed && mDirtyState & kDirtyState_IndexBuffer)
    {
        const auto buffer           = static_cast<VulkanBuffer*>(mIndexBuffer.buffer);
        const VkIndexType indexType = (mIndexBuffer.type == kGPUIndexType_32)
                                          ? VK_INDEX_TYPE_UINT32
                                          : VK_INDEX_TYPE_UINT16;

        vkCmdBindIndexBuffer(GetCommandBuffer(),
                             buffer->GetHandle(),
                             mIndexBuffer.offset,
                             indexType);

        mDirtyState &= ~kDirtyState_IndexBuffer;
    }
}

void VulkanGraphicsCommandList::Draw(const uint32_t inVertexCount,
                                     const uint32_t inFirstVertex)
{
    PreDraw(false);

    vkCmdDraw(GetCommandBuffer(),
              inVertexCount,
              1,
              inFirstVertex,
              0);
}

void VulkanGraphicsCommandList::DrawIndexed(const uint32_t inIndexCount,
                                            const uint32_t inFirstIndex,
                                            const int32_t  inVertexOffset)
{
    PreDraw(true);

    vkCmdDrawIndexed(GetCommandBuffer(),
                     inIndexCount,
                     1,
                     inFirstIndex,
                     inVertexOffset,
                     0);
}
