/*
 * Copyright (C) 2018-2019 Alex Smith
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

#include "GPU/GPUSwapchain.h"

#include "VulkanDeviceChild.h"

#include <vector>

class VulkanTexture;

class VulkanSwapchain final : public GPUSwapchain,
                              public VulkanDeviceChild<VulkanSwapchain>
{
public:
                                VulkanSwapchain(VulkanDevice& inDevice,
                                                Window&       inWindow);
                                ~VulkanSwapchain();

public:
    void                        Acquire(const VkSemaphore inAcquireSemaphore);

    void                        Present(const VkQueue     inQueue,
                                        const VkSemaphore inWaitSemaphore);

    /* Implemented by platform-specific code. */
    static const char*          GetSurfaceExtensionName();
    static bool                 CheckPresentationSupport(const VkPhysicalDevice inDevice,
                                                         const uint32_t         inQueueFamily);

private:
    /* Implemented by platform-specific code. */
    void                        CreateSurface();

    void                        ChooseFormat();
    void                        CreateSwapchain();
    void                        CreateTexture();

private:
    VkSurfaceKHR                mSurfaceHandle;
    VkSwapchainKHR              mHandle;

    VkSurfaceFormatKHR          mSurfaceFormat;

    std::vector<VkImage>        mImages;
    uint32_t                    mCurrentImage;

};
