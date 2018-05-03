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
    macro(vkGetDeviceProcAddr)

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

    #undef DECLARE_VULKAN_FUNC
}

using namespace VulkanFuncs;

/** Whether to enable the Vulkan validation layers. */
#define ORION_VULKAN_VALIDATION     ORION_BUILD_DEBUG
