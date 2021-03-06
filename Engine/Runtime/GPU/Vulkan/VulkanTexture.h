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

#pragma once

#include "GPU/GPUTexture.h"

#include "VulkanMemoryManager.h"

class VulkanSwapchain;

class VulkanTexture final : public GPUTexture,
                            public VulkanDeviceChild<VulkanTexture>
{
public:
                            VulkanTexture(VulkanDevice&         device,
                                          const GPUTextureDesc& desc);

                            VulkanTexture(VulkanSwapchain& swapchain,
                                          OnlyCalledBy<VulkanSwapchain>);

                            ~VulkanTexture();

    VkImage                 GetHandle() const           { return mHandle; }

    VkImageAspectFlags      GetAspectMask() const       { return mAspectMask; }

    /**
     * Interface with VulkanSwapchain for swapchain textures to set the current
     * swapchain image that this refers to.
     */
    void                    SetImage(const VkImage image,
                                     OnlyCalledBy<VulkanSwapchain>);

    /**
     * Flag to indicate whether a discard is pending for first use of a
     * swapchain image after acquiring (see VulkanContext::ResourceBarrier()).
     */
    bool                    GetAndResetNeedDiscard();

protected:
    void                    UpdateName() override;

private:
    VkImage                 mHandle;
    VmaAllocation           mAllocation;

    VkImageAspectFlags      mAspectMask;

    bool                    mNeedDiscard;

};

inline void VulkanTexture::SetImage(const VkImage image,
                                    OnlyCalledBy<VulkanSwapchain>)
{
    Assert(IsSwapchain());
    
    mHandle      = image;
    mNeedDiscard = true;
}

inline bool VulkanTexture::GetAndResetNeedDiscard()
{
    Assert(IsSwapchain());

    const bool result = mNeedDiscard;
    mNeedDiscard = false;
    return result;
}
