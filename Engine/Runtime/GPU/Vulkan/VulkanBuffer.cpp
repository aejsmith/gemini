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

#include "VulkanBuffer.h"

#include "VulkanDevice.h"

VulkanBuffer::VulkanBuffer(VulkanDevice&        inDevice,
                           const GPUBufferDesc& inDesc) :
    GPUBuffer   (inDevice, inDesc),
    mHandle     (VK_NULL_HANDLE),
    mAllocation (VK_NULL_HANDLE)
{
    VkBufferCreateInfo createInfo;
    createInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size        = GetSize();
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    /* These are all allowed by default. Shader read/write buffers need to be
     * flagged as such, and constants are handled separately. */
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    if (GetUsage() & kGPUResourceUsage_ShaderRead)
    {
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }

    if (GetUsage() & kGPUResourceUsage_ShaderWrite)
    {
        createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.pUserData = this;
    allocationInfo.usage     = VMA_MEMORY_USAGE_GPU_ONLY;

    GetVulkanDevice().GetMemoryManager().AllocateBuffer(createInfo,
                                                        allocationInfo,
                                                        mHandle,
                                                        mAllocation);
}

VulkanBuffer::~VulkanBuffer()
{
    GetVulkanDevice().AddFrameCompleteCallback(
        [handle = mHandle, allocation = mAllocation] (VulkanDevice& inDevice)
        {
            vkDestroyBuffer(inDevice.GetHandle(), handle, nullptr);
            inDevice.GetMemoryManager().Free(allocation);
        });
}
