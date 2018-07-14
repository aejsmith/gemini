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

#include "GPU/GPUTexture.h"

#include "VulkanMemoryManager.h"

class VulkanSwapchain;

class VulkanTexture final : public GPUTexture,
                            public VulkanDeviceChild<VulkanTexture>
{
public:
                                VulkanTexture(VulkanDevice&         inDevice,
                                              const GPUTextureDesc& inDesc);

                                VulkanTexture(VulkanSwapchain&      inSwapchain,
                                              const GPUTextureDesc& inDesc,
                                              OnlyCalledBy<VulkanSwapchain>);

                                ~VulkanTexture();

    VkImage                     GetHandle() const           { return mHandle; }

    bool                        IsSwapchainImage() const    { return mSwapchain != nullptr; }

private:
    VkImage                     mHandle;
    VmaAllocation               mAllocation;

    /** If not null, this texture is a bridge to a swapchain image. */
    VulkanSwapchain*            mSwapchain;

};
