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

#include "VulkanCommandPool.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"

template <typename T>
inline VulkanCommandList<T>::VulkanCommandList() :
    mCommandBuffer (VK_NULL_HANDLE)
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
    GetT().ValidateCommand();

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

void VulkanGraphicsCommandList::SetPipeline(GPUPipeline* const inPipeline)
{
    
}
