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

#include "VulkanInstance.h"

#include "Core/Utility.h"

#include <string>
#include <vector>
#include <unordered_set>

static const char* kRequiredInstanceExtensions[] =
{
    VK_KHR_SURFACE_EXTENSION_NAME,
};

SINGLETON_IMPL(VulkanInstance);

VulkanInstance::VulkanInstance() :
    mLoaderHandle   (nullptr),
    mHandle         (VK_NULL_HANDLE)
{
    OpenLoader();
    CreateInstance();
}

VulkanInstance::~VulkanInstance()
{
    if (mHandle != VK_NULL_HANDLE)
    {
        vkDestroyInstance(mHandle, nullptr);
    }

    CloseLoader();
}

void VulkanInstance::CreateInstance()
{
    VkResult result;

    #define LOAD_VULKAN_INSTANCE_FUNC(name) \
        name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(mHandle, #name)); \
        if (!name) \
        { \
            Fatal("Failed to load Vulkan function '" #name "'"); \
        }

    /*
     * Load all functions we need to create the instance. mHandle is null
     * initially.
     */
    ENUMERATE_VULKAN_NO_INSTANCE_FUNCS(LOAD_VULKAN_INSTANCE_FUNC);

    /* Vulkan 1.1 is required. */
    uint32_t apiVersion;
    result = vkEnumerateInstanceVersion(&apiVersion);
    if (result != VK_SUCCESS)
    {
        Fatal("Failed to get Vulkan instance version: %d", result);
    }
    else if (apiVersion < VK_API_VERSION_1_1)
    {
        Fatal("Vulkan API version 1.1 is not supported");
    }

    /*
     * Determine the instance layers/extensions to use.
     */

    uint32_t count  = 0;
    result = vkEnumerateInstanceLayerProperties(&count, nullptr);
    if (result != VK_SUCCESS)
    {
        Fatal("Failed to enumerate Vulkan instance layers (1): %d", result);
    }

    std::vector<VkLayerProperties> layerProps(count);
    result = vkEnumerateInstanceLayerProperties(&count, &layerProps[0]);
    if (result != VK_SUCCESS)
    {
        Fatal("Failed to enumerate Vulkan instance layers (2): %d", result);
    }

    std::unordered_set<std::string> availableLayers;
    LogInfo("Vulkan instance layers:");
    for (const auto& layer : layerProps)
    {
        LogInfo("  %s (spec version %u.%u.%u, revision %u)",
                layer.layerName,
                VK_VERSION_MAJOR(layer.specVersion),
                VK_VERSION_MINOR(layer.specVersion),
                VK_VERSION_PATCH(layer.specVersion),
                layer.implementationVersion);

        availableLayers.insert(layer.layerName);
    }

    result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (result != VK_SUCCESS)
    {
        Fatal("Failed to enumerate Vulkan instance extensions (1): %d", result);
    }

    std::vector<VkExtensionProperties> extensionProps(count);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &count, &extensionProps[0]);
    if (result != VK_SUCCESS)
    {
        Fatal("Failed to enumerate Vulkan instance extensions (2): %d", result);
    }

    std::unordered_set<std::string> availableExtensions;
    LogInfo("Vulkan instance extensions:");
    for (const auto& extension : extensionProps)
    {
        LogInfo("  %s (revision %u)",
                extension.extensionName,
                extension.specVersion);

        availableExtensions.insert(extension.extensionName);
    }

    std::vector<const char*> enabledLayers;
    std::vector<const char*> enabledExtensions;

    /* TODO: Check for platform surface extension. */
    enabledExtensions.assign(&kRequiredInstanceExtensions[0],
                             &kRequiredInstanceExtensions[ArraySize(kRequiredInstanceExtensions)]);

    for (const char* extension : enabledExtensions)
    {
        if (availableExtensions.find(extension) == availableExtensions.end())
        {
            Fatal("Required Vulkan instance extension '%s' not available", extension);
        }
    }

    /* Enable validation extensions if requested and present. */
    #if ORION_VULKAN_VALIDATION
        auto validationLayer = availableLayers.find("VK_LAYER_LUNARG_standard_validation");
        auto reportExtension = availableExtensions.find(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

        if (validationLayer != availableLayers.end() && reportExtension != availableExtensions.end())
        {
            LogInfo("Enabling Vulkan validation layers");
            enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
            enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }
        else
        {
            LogWarning("Vulkan validation layers are not present, not enabling");
        }
    #endif

    /* Create the instance. TODO: Get application name from Engine. */
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Orion";
    applicationInfo.pEngineName      = "Orion";
    applicationInfo.apiVersion       = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &applicationInfo;
    createInfo.enabledLayerCount       = enabledLayers.size();
    createInfo.ppEnabledLayerNames     = enabledLayers.data();
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    result = vkCreateInstance(&createInfo, nullptr, &mHandle);
    if (result != VK_SUCCESS)
    {
        Fatal("Failed to create Vulkan instance: %d", result);
    }

    /* Get instance function pointers. */
    ENUMERATE_VULKAN_INSTANCE_FUNCS(LOAD_VULKAN_INSTANCE_FUNC);
}
