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

#pragma once

#include "Core/CoreDefs.h"

#include "Engine/Profiler.h"

/* We load function pointers ourselves. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VULKAN_PROFILER_SCOPE(timer)    PROFILER_SCOPE("Vulkan", timer, 0xffff00)
#define VULKAN_PROFILER_FUNC_SCOPE()    PROFILER_FUNC_SCOPE("Vulkan", 0xffff00)

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
    macro(vkGetPhysicalDeviceSparseImageFormatProperties) \
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
    macro(vkQueuePresentKHR) \
    macro(vkGetImageMemoryRequirements2) \
    macro(vkGetBufferMemoryRequirements2)

/**
 * Instance extension functions.
 */
#define ENUMERATE_VULKAN_INSTANCE_EXTENSION_FUNCS(macro) \
    macro(vkCreateDebugReportCallbackEXT) \
    macro(vkDestroyDebugReportCallbackEXT) \
    macro(vkDebugReportMessageEXT)

/**
 * Device extension functions.
 */
#define ENUMERATE_VULKAN_DEVICE_EXTENSION_FUNCS(macro) \
    macro(vkDebugMarkerSetObjectTagEXT) \
    macro(vkDebugMarkerSetObjectNameEXT) \
    macro(vkCmdDebugMarkerBeginEXT) \
    macro(vkCmdDebugMarkerEndEXT) \
    macro(vkCmdDebugMarkerInsertEXT) \
    macro(vkResetQueryPoolEXT)

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
    ENUMERATE_VULKAN_INSTANCE_EXTENSION_FUNCS(DECLARE_VULKAN_FUNC);
    ENUMERATE_VULKAN_DEVICE_FUNCS(DECLARE_VULKAN_FUNC);
    ENUMERATE_VULKAN_DEVICE_EXTENSION_FUNCS(DECLARE_VULKAN_FUNC);

    #undef DECLARE_VULKAN_FUNC
}

using namespace VulkanFuncs;

/**
 * Vulkan parameters.
 */

/**
 * Whether to enable the Vulkan validation layers. Don't enable on sanitizer
 * builds as the layers currently cause a lot of leak errors that get in the
 * way of being able to see stuff that we care about.
 */
#define GEMINI_VULKAN_VALIDATION    (GEMINI_BUILD_DEBUG && !GEMINI_SANITIZE)

/**
 * Number of in-flight frames allowed. Currently 2: previous frame is left to
 * complete on the GPU while we're preparing the next one on the CPU.
 */
static constexpr uint8_t kVulkanInFlightFrameCount = 2;

/**
 * Maximum number of contexts (one graphics, one compute, one transfer).
 */
static constexpr uint8_t kVulkanMaxContexts = 3;

/**
 * Helper functions/macros.
 */

[[noreturn]] extern void VulkanCheckFailed(const char*    call,
                                           const VkResult result);

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

static constexpr VkAccessFlags kVkAccessFlagBits_AllRead =
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
    VK_ACCESS_INDEX_READ_BIT |
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    VK_ACCESS_UNIFORM_READ_BIT |
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
    VK_ACCESS_SHADER_READ_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    VK_ACCESS_TRANSFER_READ_BIT |
    VK_ACCESS_HOST_READ_BIT |
    VK_ACCESS_MEMORY_READ_BIT;

static constexpr VkAccessFlags kVkAccessFlagBits_AllWrite =
    VK_ACCESS_SHADER_WRITE_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_TRANSFER_WRITE_BIT |
    VK_ACCESS_HOST_WRITE_BIT |
    VK_ACCESS_MEMORY_WRITE_BIT;
