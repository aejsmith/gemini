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

#include "VulkanDeviceChild.h"

#include <deque>

class VulkanCommandPool : public GPUDeviceChild,
                          public VulkanDeviceChild<VulkanCommandPool>
{
public:
                                VulkanCommandPool(VulkanDevice&  inDevice,
                                                  const uint32_t inQueueFamily);
                                ~VulkanCommandPool();

public:
    VkCommandBuffer             AllocatePrimary();
    VkCommandBuffer             AllocateSecondary();

    void                        Reset();

private:
    using CommandBufferList   = std::deque<VkCommandBuffer>;

private:
    VkCommandPool               mHandle;

    /**
     * List of free command buffers. Resetting a command pool does not free
     * the individual command buffers allocated from it, so after a reset we
     * return all command buffers from the allocated lists to the free lists
     * to be used again. This also avoids overhead of repeatedly allocating
     * new command buffers from the driver.
     */
    CommandBufferList           mFreePrimary;
    CommandBufferList           mFreeSecondary;

    CommandBufferList           mAllocatedPrimary;
    CommandBufferList           mAllocatedSecondary;

};
