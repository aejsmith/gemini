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

#include "VulkanSwapchain.h"

#include "Core/String.h"
#include "Core/Utility.h"

#include "Engine/Game.h"

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
    mHandle         (VK_NULL_HANDLE),
    mCaps           (0)
{
    OpenLoader();
    CreateInstance();
}

VulkanInstance::~VulkanInstance()
{
    #if ORION_VULKAN_VALIDATION
        if (mCaps & kCap_DebugReport)
        {
            vkDestroyDebugReportCallbackEXT(mHandle, mDebugReportCallback, nullptr);
        }
    #endif

    if (mHandle != VK_NULL_HANDLE)
    {
        vkDestroyInstance(mHandle, nullptr);
    }

    CloseLoader();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugReportCallback(VkDebugReportFlagsEXT      flags,
                    VkDebugReportObjectTypeEXT objectType,
                    uint64_t                   object,
                    size_t                     location,
                    int32_t                    messageCode,
                    const char*                pLayerPrefix,
                    const char*                pMessage,
                    void*                      pUserData)
{
    LogLevel level = kLogLevel_Debug;
    std::string flagsString;

    if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
        flagsString += StringUtils::Format("%sDEBUG", (!flagsString.empty()) ? " | " : "");
    }

    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        flagsString += StringUtils::Format("%sINFORMATION", (!flagsString.empty()) ? " | " : "");
    }

    if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        flagsString += StringUtils::Format("%sWARNING", (!flagsString.empty()) ? " | " : "");
        level = kLogLevel_Warning;
    }

    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        flagsString += StringUtils::Format("%sPERFORMANCE", (!flagsString.empty()) ? " | " : "");
        level = kLogLevel_Warning;
    }

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        flagsString += StringUtils::Format("%sERROR", (!flagsString.empty()) ? " | " : "");
        level = kLogLevel_Error;
    }

    LogImpl(level,
            __FILE__, __LINE__,
            "Vulkan [layer = %s, flags = %s, object = 0x%" PRIx64 ", location = %zu, messageCode = %d]:",
            pLayerPrefix, flagsString.c_str(), object, location, messageCode);
    LogImpl(level,
            nullptr, 0,
            "%s",
            pMessage);

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        Fatal("Vulkan validation error (see log for details)");
    }

    return VK_FALSE;
}

void VulkanInstance::CreateInstance()
{
    #define LOAD_VULKAN_INSTANCE_FUNC(name) \
        name = Load<PFN_##name>(#name, true);

    /*
     * Load all functions we need to create the instance. mHandle is null
     * initially.
     */
    ENUMERATE_VULKAN_NO_INSTANCE_FUNCS(LOAD_VULKAN_INSTANCE_FUNC);

    /* Vulkan 1.1 is required. */
    uint32_t apiVersion;
    VulkanCheck(vkEnumerateInstanceVersion(&apiVersion));
    if (apiVersion < VK_API_VERSION_1_1)
    {
        Fatal("Vulkan API version 1.1 is not supported");
    }

    /*
     * Determine the instance layers/extensions to use.
     */

    uint32_t count  = 0;

    VulkanCheck(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> layerProps(count);
    VulkanCheck(vkEnumerateInstanceLayerProperties(&count, &layerProps[0]));

    std::unordered_set<std::string> availableLayers;
    LogInfo("Instance layers:");
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

    VulkanCheck(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> extensionProps(count);
    VulkanCheck(vkEnumerateInstanceExtensionProperties(nullptr, &count, extensionProps.data()));

    std::unordered_set<std::string> availableExtensions;
    LogInfo("Instance extensions:");
    for (const auto& extension : extensionProps)
    {
        LogInfo("  %s (revision %u)",
                extension.extensionName,
                extension.specVersion);

        availableExtensions.insert(extension.extensionName);
    }

    std::vector<const char*> enabledLayers;
    std::vector<const char*> enabledExtensions;

    enabledExtensions.assign(&kRequiredInstanceExtensions[0],
                             &kRequiredInstanceExtensions[ArraySize(kRequiredInstanceExtensions)]);

    enabledExtensions.emplace_back(VulkanSwapchain::GetSurfaceExtensionName());

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

            mCaps |= kCap_Validation | kCap_DebugReport;
        }
        else
        {
            LogWarning("Vulkan validation layers are not present, not enabling");
        }
    #endif

    /* Create the instance. TODO: Get application name from Engine. */
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = Game::Get().GetName();
    applicationInfo.pEngineName      = "Orion";
    applicationInfo.apiVersion       = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &applicationInfo;
    createInfo.enabledLayerCount       = enabledLayers.size();
    createInfo.ppEnabledLayerNames     = enabledLayers.data();
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VulkanCheck(vkCreateInstance(&createInfo, nullptr, &mHandle));

    /* Get instance function pointers. */
    ENUMERATE_VULKAN_INSTANCE_FUNCS(LOAD_VULKAN_INSTANCE_FUNC);

    /* Register a debug report callback. */
    #if ORION_VULKAN_VALIDATION
        if (mCaps & kCap_DebugReport)
        {
            ENUMERATE_VULKAN_DEBUG_REPORT_FUNCS(LOAD_VULKAN_INSTANCE_FUNC);

            VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
            callbackCreateInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
            callbackCreateInfo.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                                             VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                             VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            callbackCreateInfo.pfnCallback = DebugReportCallback;

            vkCreateDebugReportCallbackEXT(mHandle,
                                           &callbackCreateInfo,
                                           nullptr,
                                           &mDebugReportCallback);
        }
    #endif
}
