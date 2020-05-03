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

#include "VulkanStagingPool.h"

#include "VulkanDevice.h"

VulkanStagingPool::VulkanStagingPool(VulkanDevice& device) :
    GPUStagingPool  (device)
{
}

VulkanStagingPool::~VulkanStagingPool()
{
}

void* VulkanStagingPool::Allocate(const GPUStagingAccess access,
                                  const uint32_t         size,
                                  void*&                 outMapping)
{
    // TODO: Better implementation that creates a VkBuffer per VkDeviceMemory
    // that VMA creates, so that we don't have to allocate a VkBuffer for every
    // staging allocation. Also pool the VulkanStagingAllocation structures so
    // we don't have a new/delete every staging allocation.

    VkBufferCreateInfo createInfo = {};
    createInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size        = size;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.pUserData = this;
    allocationCreateInfo.flags     = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocationCreateInfo.usage     = (access == kGPUStagingAccess_Write)
                                         ? VMA_MEMORY_USAGE_CPU_TO_GPU
                                         : VMA_MEMORY_USAGE_GPU_TO_CPU;

    auto& memoryManager = GetVulkanDevice().GetMemoryManager();

    auto allocation = new VulkanStagingAllocation;

    memoryManager.AllocateBuffer(createInfo,
                                 allocationCreateInfo,
                                 allocation->handle,
                                 allocation->allocation);

    VmaAllocationInfo allocationInfo;
    memoryManager.GetInfo(allocation->allocation, allocationInfo);
    outMapping = reinterpret_cast<uint8_t*>(allocationInfo.pMappedData);

    return allocation;
}

void VulkanStagingPool::Free(void* const handle)
{
    auto allocation = static_cast<VulkanStagingAllocation*>(handle);

    GetVulkanDevice().AddFrameCompleteCallback(
        [handle = allocation->handle, allocation = allocation->allocation] (VulkanDevice& device)
        {
            vkDestroyBuffer(device.GetHandle(), handle, nullptr);
            device.GetMemoryManager().Free(allocation);
        });

    delete allocation;
}
