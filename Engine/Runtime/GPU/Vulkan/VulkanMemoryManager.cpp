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

#include "VulkanMemoryManager.h"

#include "VulkanDevice.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-variable"

#define VMA_ASSERT(expr)    Assert(expr)
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#pragma clang diagnostic pop

VulkanMemoryManager::VulkanMemoryManager(VulkanDevice& inDevice) :
    GPUDeviceChild (inDevice)
{
    VmaVulkanFunctions functions;
    functions.vkGetPhysicalDeviceProperties       = vkGetPhysicalDeviceProperties;
    functions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    functions.vkAllocateMemory                    = vkAllocateMemory;
    functions.vkFreeMemory                        = vkFreeMemory;
    functions.vkMapMemory                         = vkMapMemory;
    functions.vkUnmapMemory                       = vkUnmapMemory;
    functions.vkBindBufferMemory                  = vkBindBufferMemory;
    functions.vkBindImageMemory                   = vkBindImageMemory;
    functions.vkGetBufferMemoryRequirements       = vkGetBufferMemoryRequirements;
    functions.vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements;
    functions.vkCreateBuffer                      = vkCreateBuffer;
    functions.vkDestroyBuffer                     = vkDestroyBuffer;
    functions.vkCreateImage                       = vkCreateImage;
    functions.vkDestroyImage                      = vkDestroyImage;
    functions.vkGetBufferMemoryRequirements2KHR   = vkGetBufferMemoryRequirements2;
    functions.vkGetImageMemoryRequirements2KHR    = vkGetImageMemoryRequirements2;

    VmaAllocatorCreateInfo createInfo = {};
    createInfo.physicalDevice   = GetVulkanDevice().GetPhysicalDevice();
    createInfo.device           = GetVulkanDevice().GetHandle();
    createInfo.flags            = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    createInfo.pVulkanFunctions = &functions;

    VulkanCheck(vmaCreateAllocator(&createInfo, &mAllocator));
}

VulkanMemoryManager::~VulkanMemoryManager()
{
    vmaDestroyAllocator(mAllocator);
}
