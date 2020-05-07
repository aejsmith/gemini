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

#include "VulkanQueryPool.h"

#include "VulkanDevice.h"

VulkanQueryPool::VulkanQueryPool(VulkanDevice&           device,
                                 const GPUQueryPoolDesc& desc) :
    GPUQueryPool (device, desc)
{
    VkQueryPoolCreateInfo createInfo = {};
    createInfo.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryCount = desc.count;

    switch (desc.type)
    {
        case kGPUQueryType_Timestamp: createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP; break;
        case kGPUQueryType_Occlusion: createInfo.queryType = VK_QUERY_TYPE_OCCLUSION; break;

        default:
            UnreachableMsg("Unrecognised GPUQueryType");

    }

    VulkanCheck(vkCreateQueryPool(GetVulkanDevice().GetHandle(),
                                  &createInfo,
                                  nullptr,
                                  &mHandle));

    Reset(0, desc.count);
}

VulkanQueryPool::~VulkanQueryPool()
{
    vkDestroyQueryPool(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}

void VulkanQueryPool::Reset(const uint16_t start,
                            const uint16_t count)
{
    Assert(start + count <= GetCount());

    vkResetQueryPoolEXT(GetVulkanDevice().GetHandle(),
                        mHandle,
                        start,
                        count);
}

bool VulkanQueryPool::GetResults(const uint16_t start,
                                 const uint16_t count,
                                 const uint32_t flags,
                                 uint64_t*      outData)
{
    Assert(start + count <= GetCount());

    VkQueryResultFlags vkFlags = VK_QUERY_RESULT_64_BIT;

    if (flags & kGetResults_Wait)
    {
        vkFlags |= VK_QUERY_RESULT_WAIT_BIT;
    }

    VkResult result = vkGetQueryPoolResults(GetVulkanDevice().GetHandle(),
                                            mHandle,
                                            start,
                                            count,
                                            sizeof(uint64_t) * count,
                                            outData,
                                            sizeof(uint64_t),
                                            vkFlags);

    switch (result)
    {
        case VK_SUCCESS:
            if (GetType() == kGPUQueryType_Timestamp)
            {
                /* Convert timestamps to nanoseconds. */
                const VkPhysicalDeviceLimits& limits = GetVulkanDevice().GetLimits();
                if (limits.timestampPeriod != 1.0f)
                {
                    for (uint32_t i = 0; i < count; i++)
                    {
                        outData[i] = static_cast<double>(outData[i]) * limits.timestampPeriod;
                    }
                }
            }

            if (flags & kGetResults_Reset)
            {
                Reset(start, count);
            }

            return true;

        case VK_NOT_READY:
            return false;

        default:
            Fatal("vkGetQueryPoolResults failed: %d", result);

    }
}
