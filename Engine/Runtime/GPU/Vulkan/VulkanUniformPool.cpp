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

#include "VulkanUniformPool.h"

#include "VulkanDevice.h"

/** Size of the uniform pool per-frame. */
static constexpr uint32_t kPerFramePoolSize = 8 * 1024 * 1024;

VulkanUniformPool::VulkanUniformPool(VulkanDevice& inDevice) :
    GPUUniformPool  (inDevice),
    mHandle         (VK_NULL_HANDLE),
    mAllocation     (VK_NULL_HANDLE),
    mMapping        (nullptr),
    mAlignment      (GetVulkanDevice().GetLimits().minUniformBufferOffsetAlignment),
    mCurrentOffset  (GetVulkanDevice().GetCurrentFrame() * kPerFramePoolSize)
{
    VkBufferCreateInfo createInfo = {};
    createInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size        = kPerFramePoolSize * kVulkanInFlightFrameCount;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.pUserData = this;
    allocationCreateInfo.usage     = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationCreateInfo.flags     = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    auto& memoryManager = GetVulkanDevice().GetMemoryManager();

    memoryManager.AllocateBuffer(createInfo,
                                 allocationCreateInfo,
                                 mHandle,
                                 mAllocation);

    VmaAllocationInfo allocationInfo;
    memoryManager.GetInfo(mAllocation, allocationInfo);
    mMapping = reinterpret_cast<uint8_t*>(allocationInfo.pMappedData);

    GetVulkanDevice().UpdateName(mHandle,
                                 VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT,
                                 "VulkanUniformPool");
}

VulkanUniformPool::~VulkanUniformPool()
{
    vkDestroyBuffer(GetVulkanDevice().GetHandle(), mHandle, nullptr);
    GetVulkanDevice().GetMemoryManager().Free(mAllocation);
}

GPUUniforms VulkanUniformPool::Allocate(const size_t inSize,
                                        void*&       outMapping)
{
    Assert(inSize <= kMaxUniformsSize);

    /* Align up to the minimum offset alignment. This means that mCurrentOffset
     * is always suitably aligned for subsequent calls. */
    const uint32_t alignedSize = RoundUpPow2(static_cast<uint32_t>(inSize), mAlignment);

    const uint32_t offset = mCurrentOffset.fetch_add(alignedSize, std::memory_order_relaxed);

    AssertMsg(offset + alignedSize <= (GetVulkanDevice().GetCurrentFrame() + 1) * kPerFramePoolSize,
              "Uniform pool allocation exceeds per-frame pool size");

    outMapping = mMapping + offset;
    return offset;
}

void VulkanUniformPool::BeginFrame()
{
    /* Reset the offset to the start of this frame's section of the buffer. It
     * is safe to re-use this memory because we'll have waited on the frame's
     * last fence. */
    mCurrentOffset.store(GetVulkanDevice().GetCurrentFrame() * kPerFramePoolSize, std::memory_order_relaxed);
}
