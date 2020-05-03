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

#include "VulkanDescriptorPool.h"

#include "VulkanDevice.h"

/* TODO: Picked mostly arbitrarily, should allocate new pools dynamically if
 * needed. */
static const VkDescriptorPoolSize kDescriptorPoolSizes[] =
{
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 4096 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         512  },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          8192 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          512  },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   512  },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   512  },
    { VK_DESCRIPTOR_TYPE_SAMPLER,                4096 }
};

static constexpr uint32_t kDescriptorPoolMaxSets = 4096;

VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice& device) :
    GPUDeviceChild  (device),
    mHandle         (VK_NULL_HANDLE)
{
    VkDescriptorPoolCreateInfo createInfo = {};
    createInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    createInfo.maxSets       = kDescriptorPoolMaxSets;
    createInfo.poolSizeCount = ArraySize(kDescriptorPoolSizes);
    createInfo.pPoolSizes    = kDescriptorPoolSizes;

    VulkanCheck(vkCreateDescriptorPool(GetVulkanDevice().GetHandle(),
                                       &createInfo,
                                       nullptr,
                                       &mHandle));
}

VulkanDescriptorPool::~VulkanDescriptorPool()
{
    vkDestroyDescriptorPool(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}

VkDescriptorSet VulkanDescriptorPool::Allocate(const VkDescriptorSetLayout layout)
{
    std::unique_lock lock(mLock);

    /* TODO: Allocate new pools on demand. */
    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool     = mHandle;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts        = &layout;

    VkDescriptorSet descriptorSet;

    VulkanCheck(vkAllocateDescriptorSets(GetVulkanDevice().GetHandle(),
                                         &allocateInfo,
                                         &descriptorSet));

    return descriptorSet;
}

void VulkanDescriptorPool::Free(const VkDescriptorSet descriptorSet)
{
    std::unique_lock lock(mLock);

    VulkanCheck(vkFreeDescriptorSets(GetVulkanDevice().GetHandle(),
                                     mHandle,
                                     1,
                                     &descriptorSet));
}
