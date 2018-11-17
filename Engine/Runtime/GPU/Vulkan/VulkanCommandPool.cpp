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

#include "VulkanCommandPool.h"

#include "VulkanDevice.h"

#include <algorithm>

/* TODO: Picked mostly arbitrarily, should allocate new pools dynamically if
 * needed. */
static const VkDescriptorPoolSize kDynamicDescriptorPoolSizes[] =
{
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         128  },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          2048 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          128  },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   128  },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   128  },
    { VK_DESCRIPTOR_TYPE_SAMPLER,                1024 }
};

static constexpr uint32_t kDynamicDescriptorPoolMaxSets = 1024;

VulkanCommandPool::VulkanCommandPool(VulkanDevice&  inDevice,
                                     const uint32_t inQueueFamily) :
    GPUDeviceChild (inDevice)
{
    /* Our command buffers are all dynamically created and reset per-frame, so
     * set the transient flag. */
    VkCommandPoolCreateInfo commandCreateInfo = {};
    commandCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandCreateInfo.queueFamilyIndex = inQueueFamily;
    commandCreateInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VulkanCheck(vkCreateCommandPool(GetVulkanDevice().GetHandle(),
                                    &commandCreateInfo,
                                    nullptr,
                                    &mCommandPool));

    /* We reset this pool in one go, don't need to free individual sets. */
    VkDescriptorPoolCreateInfo descriptorCreateInfo = {};
    descriptorCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorCreateInfo.maxSets       = kDynamicDescriptorPoolMaxSets;
    descriptorCreateInfo.poolSizeCount = ArraySize(kDynamicDescriptorPoolSizes);
    descriptorCreateInfo.pPoolSizes    = kDynamicDescriptorPoolSizes;

    VulkanCheck(vkCreateDescriptorPool(GetVulkanDevice().GetHandle(),
                                       &descriptorCreateInfo,
                                       nullptr,
                                       &mDescriptorPool));
}

VulkanCommandPool::~VulkanCommandPool()
{
    vkDestroyDescriptorPool(GetVulkanDevice().GetHandle(), mDescriptorPool, nullptr);
    vkDestroyCommandPool(GetVulkanDevice().GetHandle(), mCommandPool, nullptr);
}

VkCommandBuffer VulkanCommandPool::AllocatePrimary()
{
    VkCommandBuffer commandBuffer;

    if (!mFreePrimary.empty())
    {
        commandBuffer = mFreePrimary.front();
        mFreePrimary.pop_front();
    }
    else
    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool        = mCommandPool;
        allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        VulkanCheck(vkAllocateCommandBuffers(GetVulkanDevice().GetHandle(),
                                             &allocateInfo,
                                             &commandBuffer));
    }

    mAllocatedPrimary.push_back(commandBuffer);

    return commandBuffer;
}

VkCommandBuffer VulkanCommandPool::AllocateSecondary()
{
    VkCommandBuffer commandBuffer;

    if (!mFreeSecondary.empty())
    {
        commandBuffer = mFreeSecondary.front();
        mFreeSecondary.pop_front();
    }
    else
    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool        = mCommandPool;
        allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocateInfo.commandBufferCount = 1;

        VulkanCheck(vkAllocateCommandBuffers(GetVulkanDevice().GetHandle(),
                                             &allocateInfo,
                                             &commandBuffer));
    }

    mAllocatedSecondary.push_back(commandBuffer);

    return commandBuffer;
}

VkDescriptorSet VulkanCommandPool::AllocateDescriptorSet(const VkDescriptorSetLayout inLayout)
{
    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool     = mDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts        = &inLayout;

    VkDescriptorSet descriptorSet;

    VulkanCheck(vkAllocateDescriptorSets(GetVulkanDevice().GetHandle(),
                                         &allocateInfo,
                                         &descriptorSet));

    return descriptorSet;
}

void VulkanCommandPool::Reset()
{
    VulkanCheck(vkResetCommandPool(GetVulkanDevice().GetHandle(), mCommandPool, 0));
    VulkanCheck(vkResetDescriptorPool(GetVulkanDevice().GetHandle(), mDescriptorPool, 0));

    /* All command buffers that were allocated have now been reset and can be
     * used again. */
    std::move(mAllocatedPrimary.begin(), mAllocatedPrimary.end(), std::back_inserter(mFreePrimary));
    std::move(mAllocatedSecondary.begin(), mAllocatedSecondary.end(), std::back_inserter(mFreeSecondary));
}
