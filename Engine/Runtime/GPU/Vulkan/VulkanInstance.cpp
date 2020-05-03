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

#if GEMINI_VULKAN_VALIDATION
static bool sBreakOnValidationErrors = true;
#endif

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
    #if GEMINI_VULKAN_VALIDATION
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

#if GEMINI_VULKAN_VALIDATION

static const char* const kMessageFilters[] =
{
    /* VMA causes this when mapping allocations containing both buffers and
     * images but it is fine to ignore. */
    "Mapping an image with layout",
};

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

    for (const char* filter : kMessageFilters)
    {
        if (strstr(pMessage, filter))
        {
            return VK_FALSE;
        }
    }

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

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT && sBreakOnValidationErrors)
    {
        DebugBreak();
    }

    return VK_FALSE;
}

#endif

void VulkanInstance::CreateInstance()
{
    #define LOAD_VULKAN_INSTANCE_FUNC(name) \
        name = Load<PFN_##name>(#name, true);

    #define LOAD_OPTIONAL_VULKAN_INSTANCE_FUNC(name) \
        name = Load<PFN_##name>(#name, false);

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

    std::unordered_set<std::string> availableExtensions;

    auto EnumerateExtensions =
        [&] (const char* const layerName)
        {
            VulkanCheck(vkEnumerateInstanceExtensionProperties(layerName, &count, nullptr));
            std::vector<VkExtensionProperties> extensionProps(count);
            VulkanCheck(vkEnumerateInstanceExtensionProperties(layerName, &count, extensionProps.data()));

            for (const auto& extension : extensionProps)
            {
                LogInfo("  %s (revision %u)",
                        extension.extensionName,
                        extension.specVersion);

                availableExtensions.insert(extension.extensionName);
            }
        };

    LogInfo("Instance extensions:");
    EnumerateExtensions(nullptr);

    auto EnableLayer =
        [&] (const char* const name, const uint32_t cap) -> bool
        {
            const bool available = availableLayers.find(name) != availableLayers.end();

            if (available)
            {
                mEnabledLayers.emplace_back(name);
                mCaps |= cap;

                /* Get extensions provided by the layer. */
                LogInfo("Instance extensions (%s):", name);
                EnumerateExtensions(name);
            }

            return available;
        };

    Unused(EnableLayer);

    std::vector<const char*> enabledExtensions;

    auto EnableExtension =
        [&] (const char* const name, const uint32_t cap, const bool required = false) -> bool
        {
            const bool available = availableExtensions.find(name) != availableExtensions.end();

            if (available)
            {
                enabledExtensions.emplace_back(name);
                mCaps |= cap;
            }
            else if (required)
            {
                Fatal("Required Vulkan instance extension '%s' not available", name);
            }

            return available;
        };

    EnableExtension(VulkanSwapchain::GetSurfaceExtensionName(), 0, true);

    for (const char* const extension : kRequiredInstanceExtensions)
    {
        EnableExtension(extension, 0, true);
    }

    /* Enable validation extensions if requested and present. */
    #if GEMINI_VULKAN_VALIDATION
        const char* const env = getenv("GEMINI_VULKAN_VALIDATION");
        bool enable = !env || strcmp(env, "0") != 0;

        if (enable)
        {
            if (EnableLayer("VK_LAYER_KHRONOS_validation", kCap_Validation))
            {
                LogInfo("Enabling Vulkan validation layers");

                if (!EnableExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, kCap_DebugReport))
                {
                    LogWarning("Vulkan validation layers are enabled, but debug report unavailable");
                }
            }
            else
            {
                LogWarning("Vulkan validation layers are not present, not enabling");
            }
        }
    #endif

    /* Create the instance. TODO: Get application name from Engine. */
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = Game::Get().GetName();
    applicationInfo.pEngineName      = "Gemini";
    applicationInfo.apiVersion       = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &applicationInfo;
    createInfo.enabledLayerCount       = mEnabledLayers.size();
    createInfo.ppEnabledLayerNames     = mEnabledLayers.data();
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VulkanCheck(vkCreateInstance(&createInfo, nullptr, &mHandle));

    /* Get instance function pointers. */
    ENUMERATE_VULKAN_INSTANCE_FUNCS(LOAD_VULKAN_INSTANCE_FUNC);
    ENUMERATE_VULKAN_INSTANCE_EXTENSION_FUNCS(LOAD_OPTIONAL_VULKAN_INSTANCE_FUNC);

    /* Register a debug report callback. */
    #if GEMINI_VULKAN_VALIDATION
        if (HasCap(kCap_DebugReport))
        {
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
