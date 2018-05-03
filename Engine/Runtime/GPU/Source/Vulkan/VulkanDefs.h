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

#pragma once

#include "Core/CoreDefs.h"

/* We load function pointers ourselves. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

/**
 * All functions that we can obtain from vkGetInstanceProcAddr without an
 * instance handle.
 */
#define ENUMERATE_VULKAN_NO_INSTANCE_FUNCS(macro) \
    macro(vkEnumerateInstanceExtensionProperties) \
    macro(vkEnumerateInstanceLayerProperties) \
    macro(vkEnumerateInstanceVersion) \
    macro(vkCreateInstance)

/**
 * Non-device-specific functions that we can obtain from vkGetInstanceProcAddr
 * with an instance handle.
 */
#define ENUMERATE_VULKAN_INSTANCE_FUNCS(macro) \
    macro(vkDestroyInstance) \
    macro(vkEnumeratePhysicalDevices) \
    macro(vkGetPhysicalDeviceFeatures) \
    macro(vkGetPhysicalDeviceFormatProperties) \
    macro(vkGetPhysicalDeviceImageFormatProperties) \
    macro(vkGetPhysicalDeviceProperties) \
    macro(vkGetPhysicalDeviceQueueFamilyProperties) \
    macro(vkGetPhysicalDeviceMemoryProperties) \
    macro(vkEnumerateDeviceExtensionProperties) \
    macro(vkEnumerateDeviceLayerProperties) \
    macro(vkCreateDevice) \
    macro(vkDestroyDevice) \
    macro(vkGetDeviceProcAddr) \
    macro(vkDestroySurfaceKHR) \
    macro(vkGetPhysicalDeviceSurfaceSupportKHR) \
    macro(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    macro(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    macro(vkGetPhysicalDeviceSurfacePresentModesKHR)

/**
 * Device-specific functions obtained from vkGetDeviceProcAddr.
 */
#define ENUMERATE_VULKAN_DEVICE_FUNCS(macro) \
    macro(vkGetDeviceQueue) \
    macro(vkQueueSubmit) \
    macro(vkQueueWaitIdle) \
    macro(vkDeviceWaitIdle) \
    macro(vkAllocateMemory) \
    macro(vkFreeMemory) \
    macro(vkMapMemory) \
    macro(vkUnmapMemory) \
    macro(vkFlushMappedMemoryRanges) \
    macro(vkInvalidateMappedMemoryRanges) \
    macro(vkGetDeviceMemoryCommitment) \
    macro(vkBindBufferMemory) \
    macro(vkBindImageMemory) \
    macro(vkGetBufferMemoryRequirements) \
    macro(vkGetImageMemoryRequirements) \
    macro(vkGetImageSparseMemoryRequirements) \
    macro(vkGetPhysicalDeviceSparseImageFormatProperties) \
    macro(vkQueueBindSparse) \
    macro(vkCreateFence) \
    macro(vkDestroyFence) \
    macro(vkResetFences) \
    macro(vkGetFenceStatus) \
    macro(vkWaitForFences) \
    macro(vkCreateSemaphore) \
    macro(vkDestroySemaphore) \
    macro(vkCreateEvent) \
    macro(vkDestroyEvent) \
    macro(vkGetEventStatus) \
    macro(vkSetEvent) \
    macro(vkResetEvent) \
    macro(vkCreateQueryPool) \
    macro(vkDestroyQueryPool) \
    macro(vkGetQueryPoolResults) \
    macro(vkCreateBuffer) \
    macro(vkDestroyBuffer) \
    macro(vkCreateBufferView) \
    macro(vkDestroyBufferView) \
    macro(vkCreateImage) \
    macro(vkDestroyImage) \
    macro(vkGetImageSubresourceLayout) \
    macro(vkCreateImageView) \
    macro(vkDestroyImageView) \
    macro(vkCreateShaderModule) \
    macro(vkDestroyShaderModule) \
    macro(vkCreatePipelineCache) \
    macro(vkDestroyPipelineCache) \
    macro(vkGetPipelineCacheData) \
    macro(vkMergePipelineCaches) \
    macro(vkCreateGraphicsPipelines) \
    macro(vkCreateComputePipelines) \
    macro(vkDestroyPipeline) \
    macro(vkCreatePipelineLayout) \
    macro(vkDestroyPipelineLayout) \
    macro(vkCreateSampler) \
    macro(vkDestroySampler) \
    macro(vkCreateDescriptorSetLayout) \
    macro(vkDestroyDescriptorSetLayout) \
    macro(vkCreateDescriptorPool) \
    macro(vkDestroyDescriptorPool) \
    macro(vkResetDescriptorPool) \
    macro(vkAllocateDescriptorSets) \
    macro(vkFreeDescriptorSets) \
    macro(vkUpdateDescriptorSets) \
    macro(vkCreateFramebuffer) \
    macro(vkDestroyFramebuffer) \
    macro(vkCreateRenderPass) \
    macro(vkDestroyRenderPass) \
    macro(vkGetRenderAreaGranularity) \
    macro(vkCreateCommandPool) \
    macro(vkDestroyCommandPool) \
    macro(vkResetCommandPool) \
    macro(vkAllocateCommandBuffers) \
    macro(vkFreeCommandBuffers) \
    macro(vkBeginCommandBuffer) \
    macro(vkEndCommandBuffer) \
    macro(vkResetCommandBuffer) \
    macro(vkCmdBindPipeline) \
    macro(vkCmdSetViewport) \
    macro(vkCmdSetScissor) \
    macro(vkCmdSetLineWidth) \
    macro(vkCmdSetDepthBias) \
    macro(vkCmdSetBlendConstants) \
    macro(vkCmdSetDepthBounds) \
    macro(vkCmdSetStencilCompareMask) \
    macro(vkCmdSetStencilWriteMask) \
    macro(vkCmdSetStencilReference) \
    macro(vkCmdBindDescriptorSets) \
    macro(vkCmdBindIndexBuffer) \
    macro(vkCmdBindVertexBuffers) \
    macro(vkCmdDraw) \
    macro(vkCmdDrawIndexed) \
    macro(vkCmdDrawIndirect) \
    macro(vkCmdDrawIndexedIndirect) \
    macro(vkCmdDispatch) \
    macro(vkCmdDispatchIndirect) \
    macro(vkCmdCopyBuffer) \
    macro(vkCmdCopyImage) \
    macro(vkCmdBlitImage) \
    macro(vkCmdCopyBufferToImage) \
    macro(vkCmdCopyImageToBuffer) \
    macro(vkCmdUpdateBuffer) \
    macro(vkCmdFillBuffer) \
    macro(vkCmdClearColorImage) \
    macro(vkCmdClearDepthStencilImage) \
    macro(vkCmdClearAttachments) \
    macro(vkCmdResolveImage) \
    macro(vkCmdSetEvent) \
    macro(vkCmdResetEvent) \
    macro(vkCmdWaitEvents) \
    macro(vkCmdPipelineBarrier) \
    macro(vkCmdBeginQuery) \
    macro(vkCmdEndQuery) \
    macro(vkCmdResetQueryPool) \
    macro(vkCmdWriteTimestamp) \
    macro(vkCmdCopyQueryPoolResults) \
    macro(vkCmdPushConstants) \
    macro(vkCmdBeginRenderPass) \
    macro(vkCmdNextSubpass) \
    macro(vkCmdEndRenderPass) \
    macro(vkCmdExecuteCommands) \
    macro(vkCreateSwapchainKHR) \
    macro(vkDestroySwapchainKHR) \
    macro(vkGetSwapchainImagesKHR) \
    macro(vkAcquireNextImageKHR) \
    macro(vkQueuePresentKHR)

/**
 * Table of Vulkan function pointers loaded manually. These are made available
 * under their usual name by the using statement below. We load all device
 * functions from our created device: our device is a singleton, so we can
 * always just use the function pointers directly from that device globally.
 */
namespace VulkanFuncs
{
    /* Obtained via a platform-specific method. */
    extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

    #define DECLARE_VULKAN_FUNC(name) \
        extern PFN_##name name;

    ENUMERATE_VULKAN_NO_INSTANCE_FUNCS(DECLARE_VULKAN_FUNC);
    ENUMERATE_VULKAN_INSTANCE_FUNCS(DECLARE_VULKAN_FUNC);
    ENUMERATE_VULKAN_DEVICE_FUNCS(DECLARE_VULKAN_FUNC);

    #undef DECLARE_VULKAN_FUNC
}

using namespace VulkanFuncs;

/** Whether to enable the Vulkan validation layers. */
#define ORION_VULKAN_VALIDATION     ORION_BUILD_DEBUG

[[noreturn]] extern void VulkanCheckFailed(const char*    inCall,
                                           const VkResult inResult);

/** Handle failure of a Vulkan call. */
#define VulkanCheck(_call) \
    do \
    { \
        VkResult _result = _call; \
        if (_result != VK_SUCCESS) \
        { \
            VulkanCheckFailed(#_call, _result); \
        } \
    } \
    while (0)
