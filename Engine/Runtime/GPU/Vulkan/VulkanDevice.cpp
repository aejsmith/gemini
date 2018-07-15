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

#include "VulkanDevice.h"

#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include "VulkanResourceView.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"

#include "Core/Utility.h"

#include <unordered_set>
#include <limits>
#include <vector>

static const char* kRequiredDeviceExtensions[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,

    /* We are targeting 1.1 and these extensions should be in core, however the
     * VulkanMemoryAllocator library uses them under their KHR aliases so
     * enable them explicitly. */
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
};

VulkanDevice::VulkanDevice() :
    mHandle (VK_NULL_HANDLE)
{
    if (!VulkanInstance::HasInstance())
    {
        /* Create the global Vulkan instance. */
        new VulkanInstance();
    }

    mInstance = &VulkanInstance::Get();

    CreateDevice();

    mMemoryManager   = new VulkanMemoryManager(*this);
    mGraphicsContext = new VulkanContext(*this, GetGraphicsQueueFamily());
}

VulkanDevice::~VulkanDevice()
{
    delete mGraphicsContext;
    delete mMemoryManager;

    vkDestroyDevice(mHandle, nullptr);
}

void VulkanDevice::CreateDevice()
{
    uint32_t count;
    VulkanCheck(vkEnumeratePhysicalDevices(GetInstance().GetHandle(), &count, nullptr));

    if (count == 0)
    {
        Fatal("No Vulkan physical devices available");
    }

    std::vector<VkPhysicalDevice> devices(count);
    VulkanCheck(vkEnumeratePhysicalDevices(GetInstance().GetHandle(), &count, devices.data()));

    mPhysicalDevice = VK_NULL_HANDLE;

    std::unordered_set<std::string> availableExtensions;
    std::vector<VkExtensionProperties> extensionProps;

    for (size_t i = 0; i < devices.size(); i++)
    {
        VkPhysicalDevice device = devices[i];

        vkGetPhysicalDeviceProperties(device, &mProperties);
        vkGetPhysicalDeviceFeatures(device, &mFeatures);

        LogInfo("Checking device %zu (%s)", i, mProperties.deviceName);

        if (mProperties.apiVersion < VK_API_VERSION_1_1)
        {
            LogWarning("  Vulkan 1.1 is not supported");
            continue;
        }

        VulkanCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr));
        extensionProps.resize(count);
        VulkanCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensionProps.data()));

        availableExtensions.clear();
        for (const VkExtensionProperties& extension : extensionProps)
        {
            availableExtensions.insert(extension.extensionName);
        }

        bool found = true;

        for (const char* extension : kRequiredDeviceExtensions)
        {
            found = availableExtensions.find(extension) != availableExtensions.end();
            if (!found)
            {
                LogWarning("  Required device extension '%s' not available", extension);
                break;
            }
        }

        if (!found)
        {
            continue;
        }

        /* Find suitable queue families. We need to support both graphics
         * operations and presentation. */
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

        mGraphicsQueueFamily = std::numeric_limits<uint32_t>::max();

        if (count > 0)
        {
            std::vector<VkQueueFamilyProperties> familyProps(count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, familyProps.data());

            for (uint32_t family = 0; family < familyProps.size(); family++)
            {
                const bool graphicsSupported = familyProps[family].queueCount > 0 &&
                                               familyProps[family].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                                               familyProps[family].queueFlags & VK_QUEUE_COMPUTE_BIT;

                const bool presentSupported  = VulkanSwapchain::CheckPresentationSupport(device, i);

                if (graphicsSupported && presentSupported)
                {
                    mGraphicsQueueFamily = family;
                    break;
                }
            }
        }

        if (mGraphicsQueueFamily == std::numeric_limits<uint32_t>::max())
        {
            LogWarning("  No suitable queue families");
            continue;
        }

        /* This device is good. */
        mPhysicalDevice = device;
        break;
    }

    if (mPhysicalDevice == VK_NULL_HANDLE)
    {
        Fatal("No suitable Vulkan devices found");
    }

    LogInfo("  API version: %u.%u.%u",
            VK_VERSION_MAJOR(mProperties.apiVersion),
            VK_VERSION_MINOR(mProperties.apiVersion),
            VK_VERSION_PATCH(mProperties.apiVersion));
    LogInfo("  IDs:         0x%x / 0x%x",
            mProperties.vendorID,
            mProperties.deviceID);

    LogInfo("  Extensions:");

    for (const VkExtensionProperties& extension : extensionProps)
    {
        LogInfo("    %s (revision %u)",
                extension.extensionName,
                extension.specVersion);
    }

    std::vector<const char*> enabledLayers;
    std::vector<const char*> enabledExtensions;

    if (GetInstance().HasCap(VulkanInstance::kCap_Validation))
    {
        /* Assume that if the instance has validation then the device does
         * too. */
        enabledLayers.emplace_back("VK_LAYER_LUNARG_standard_validation");
    }

    enabledExtensions.assign(&kRequiredDeviceExtensions[0],
                             &kRequiredDeviceExtensions[ArraySize(kRequiredDeviceExtensions)]);

    /* Enable all supported features, aside from robustBufferAccess - we
     * should behave properly without it, and it can have a performance
     * impact. */
    mFeatures.robustBufferAccess = false;

    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = mGraphicsQueueFamily;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
    deviceCreateInfo.enabledLayerCount       = enabledLayers.size();
    deviceCreateInfo.ppEnabledLayerNames     = enabledLayers.data();
    deviceCreateInfo.enabledExtensionCount   = enabledExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    deviceCreateInfo.pEnabledFeatures        = &mFeatures;;

    VulkanCheck(vkCreateDevice(mPhysicalDevice,
                               &deviceCreateInfo,
                               nullptr,
                               &mHandle));

    #define LOAD_VULKAN_DEVICE_FUNC(name) \
        name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(mHandle, #name)); \
        if (!name) \
        { \
            Fatal("Failed to load Vulkan function '%s'", #name); \
        }

    ENUMERATE_VULKAN_DEVICE_FUNCS(LOAD_VULKAN_DEVICE_FUNC);

    #undef LOAD_VULKAN_DEVICE_FUNC
}

void VulkanDevice::CreateSwapchain(Window& inWindow)
{
    new VulkanSwapchain(*this, inWindow);
}

GPUTexturePtr VulkanDevice::CreateTexture(const GPUTextureDesc& inDesc)
{
    return new VulkanTexture(*this, inDesc);
}

GPUResourceViewPtr VulkanDevice::CreateResourceView(GPUResource&               inResource,
                                                    const GPUResourceViewDesc& inDesc)
{
    return new VulkanResourceView(inResource, inDesc);
}
