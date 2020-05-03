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

#include "GPU/GPUResourceView.h"

#include "VulkanDeviceChild.h"

class VulkanSwapchain;

class VulkanResourceView final : public GPUResourceView,
                                 public VulkanDeviceChild<VulkanResourceView>
{
public:
                                VulkanResourceView(GPUResource&               resource,
                                                   const GPUResourceViewDesc& desc);

                                ~VulkanResourceView();

    VkImageView                 GetImageView() const;

    /**
     * Returns a const reference to the buffer view handle, allows us to point
     * to this directly in VulkanArgumentSet::Write() without putting it in
     * temporary intermediate storage.
     */
    const VkBufferView&         GetBufferView() const;

private:
    void                        CreateImageView();

private:
    union
    {
        VkImageView             mImageView;
        VkBufferView            mBufferView;
    };
};

inline VkImageView VulkanResourceView::GetImageView() const
{
    Assert(GetResource().IsTexture());
    return mImageView;
}

inline const VkBufferView& VulkanResourceView::GetBufferView() const
{
    Assert(GetType() == kGPUResourceViewType_TextureBuffer);
    return mBufferView;
}
