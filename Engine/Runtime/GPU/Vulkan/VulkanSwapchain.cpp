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

#include "VulkanSwapchain.h"

#include "VulkanDevice.h"
#include "VulkanFormat.h"

#include "Engine/Window.h"

static const uint32_t kNumSwapchainImages = 3;

VulkanSwapchain::VulkanSwapchain(VulkanDevice& inDevice,
                                 Window&       inWindow) :
    GPUSwapchain   (inDevice, inWindow),
    mSurfaceHandle (VK_NULL_HANDLE),
    mHandle        (VK_NULL_HANDLE)
{
    CreateSurface();
    ChooseFormat();
    CreateSwapchain();
}

VulkanSwapchain::~VulkanSwapchain()
{
    if (mHandle != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(GetVulkanDevice().GetHandle(), mHandle, nullptr);
    }

    if (mSurfaceHandle != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(GetVulkanInstance().GetHandle(), mSurfaceHandle, nullptr);
    }
}

void VulkanSwapchain::ChooseFormat()
{
    uint32_t count;
    VulkanCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(GetVulkanDevice().GetPhysicalDevice(),
                                                     mSurfaceHandle,
                                                     &count,
                                                     nullptr));

    if (count == 0)
    {
        Fatal("Vulkan surface has no formats available");
    }

    std::vector<VkSurfaceFormatKHR> formats(count);
    VulkanCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(GetVulkanDevice().GetPhysicalDevice(),
                                                     mSurfaceHandle,
                                                     &count,
                                                     formats.data()));

    mSurfaceFormat.colorSpace = formats[0].colorSpace;

    /* A single entry with undefined format means that there is no preferred
     * format, and we can choose whatever we like. */
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        mSurfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
        /* Default to the first format in the list. */
        mSurfaceFormat.format = formats[0].format;

        /* Search for our desired format. */
        for (const VkSurfaceFormatKHR& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                mSurfaceFormat.format = format.format;
            }
        }
    }

    /* Now we need to convert this back to a generic pixel format definition. */
    mFormat = VulkanFormat::GetPixelFormat(mSurfaceFormat.format);
    if (mFormat == PixelFormat::kUnknown)
    {
        Fatal("Vulkan surface format is unrecognised");
    }
}

void VulkanSwapchain::CreateSwapchain()
{
    /* We already checked for presentation support as part of device selection,
     * however the validation layers require you to explicitly check the
     * specific surface that you create. */
    VkBool32 presentationSupported;
    VulkanCheck(vkGetPhysicalDeviceSurfaceSupportKHR(GetVulkanDevice().GetPhysicalDevice(),
                                                     GetVulkanDevice().GetGraphicsQueueFamily(),
                                                     mSurfaceHandle,
                                                     &presentationSupported));
    if (!presentationSupported)
    {
        Fatal("Vulkan device does not support presentation to created surface");
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = mSurfaceHandle;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.clipped          = VK_TRUE;

    /* Get surface capabilities. */
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VulkanCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GetVulkanDevice().GetPhysicalDevice(),
                                                          mSurfaceHandle,
                                                          &surfaceCapabilities));

    /* Determine number of images. Request at least one more than the minimum
     * number of images required by the presentation engine, because that is
     * the minimum it needs to work and we want an additional one for buffering. */
    createInfo.minImageCount = glm::clamp(kNumSwapchainImages,
                                          surfaceCapabilities.minImageCount + 1,
                                          surfaceCapabilities.maxImageCount);

    /* Define swap chain image extents. If the current extent is given as -1,
     * this means the surface size will be determined by the size we give for
     * the swap chain, so set the requested window size in this case. Otherwise,
     * use what we are given. */
    if (surfaceCapabilities.currentExtent.width == static_cast<uint32_t>(-1))
    {
        createInfo.imageExtent.width  = glm::clamp(static_cast<uint32_t>(GetWindow().GetSize().x),
                                                   surfaceCapabilities.minImageExtent.width,
                                                   surfaceCapabilities.maxImageExtent.width);
        createInfo.imageExtent.height = glm::clamp(static_cast<uint32_t>(GetWindow().GetSize().y),
                                                   surfaceCapabilities.minImageExtent.height,
                                                   surfaceCapabilities.maxImageExtent.height);
    }
    else
    {
        createInfo.imageExtent = surfaceCapabilities.currentExtent;
    }

    /* Determine presentation transformation. */
    createInfo.preTransform =
        (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : surfaceCapabilities.currentTransform;

    createInfo.imageColorSpace = mSurfaceFormat.colorSpace;
    createInfo.imageFormat     = mSurfaceFormat.format;

    /* Get presentation modes. */
    uint32_t count;
    VulkanCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(GetVulkanDevice().GetPhysicalDevice(),
                                                          mSurfaceHandle,
                                                          &count,
                                                          nullptr));

    if (count == 0)
    {
        Fatal("No Vulkan presentation modes available");
    }

    std::vector<VkPresentModeKHR> presentModes(count);
    VulkanCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(GetVulkanDevice().GetPhysicalDevice(),
                                                          mSurfaceHandle,
                                                          &count,
                                                          presentModes.data()));

    /* FIFO mode (v-sync) should always be present. */
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    /* FIXME: Make v-sync configurable. */
    for (size_t i = 0; i < presentModes.size(); i++)
    {
        if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    VulkanCheck(vkCreateSwapchainKHR(GetVulkanDevice().GetHandle(),
                                     &createInfo,
                                     nullptr,
                                     &mHandle));

    VulkanCheck(vkGetSwapchainImagesKHR(GetVulkanDevice().GetHandle(),
                                        mHandle,
                                        &count,
                                        nullptr));

    mImages.resize(count);

    VulkanCheck(vkGetSwapchainImagesKHR(GetVulkanDevice().GetHandle(),
                                        mHandle,
                                        &count,
                                        mImages.data()));
}