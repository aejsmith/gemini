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

#include "GPU/GPUDevice.h"

#include "VulkanInstance.h"

class VulkanMemoryManager;

class VulkanDevice final : public GPUDevice
{
public:
                                        VulkanDevice();
                                        ~VulkanDevice();

    /**
     * GPUDevice methods.
     */
public:
    void                                CreateSwapchain(Window& inWindow) override;

    /**
     * Internal methods.
     */
public:
    VulkanInstance&                     GetInstance() const             { return *mInstance; }

    VkPhysicalDevice                    GetPhysicalDevice() const       { return mPhysicalDevice; }
    VkDevice                            GetHandle() const               { return mHandle; }
    uint32_t                            GetGraphicsQueueFamily() const  { return mGraphicsQueueFamily; }
    const VkPhysicalDeviceProperties&   GetProperties() const           { return mProperties; }
    const VkPhysicalDeviceLimits&       GetLimits() const               { return mProperties.limits; }
    const VkPhysicalDeviceFeatures&     GetFeatures() const             { return mFeatures; }

    VulkanMemoryManager&                GetMemoryManager() const        { return *mMemoryManager; }

private:
    void                                CreateDevice();

private:
    VulkanInstance*                     mInstance;

    VkPhysicalDevice                    mPhysicalDevice;
    VkDevice                            mHandle;
    uint32_t                            mGraphicsQueueFamily;
    VkPhysicalDeviceProperties          mProperties;
    VkPhysicalDeviceFeatures            mFeatures;

    VulkanMemoryManager*                mMemoryManager;

};