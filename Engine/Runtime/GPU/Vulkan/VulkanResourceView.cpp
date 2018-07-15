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

#include "VulkanResourceView.h"

#include "VulkanDevice.h"
#include "VulkanFormat.h"
#include "VulkanTexture.h"

VulkanResourceView::VulkanResourceView(GPUResource&               inResource,
                                       const GPUResourceViewDesc& inDesc) :
    GPUResourceView (inResource, inDesc)
{
    mImageView  = VK_NULL_HANDLE;
    mBufferView = VK_NULL_HANDLE;

    if (GetType() == kGPUResourceViewType_TextureBuffer)
    {
        Fatal("TODO: Texture buffer views");
    }
    else if (GetType() != kGPUResourceViewType_Buffer)
    {
        auto& texture = static_cast<VulkanTexture&>(GetResource());

        /* Swapchain texture views have special handling. */
        if (!texture.IsSwapchain())
        {
            CreateImageView();
        }
    }
}

VulkanResourceView::~VulkanResourceView()
{
    if (GetType() == kGPUResourceViewType_TextureBuffer)
    {
        vkDestroyBufferView(GetVulkanDevice().GetHandle(), mBufferView, nullptr);
    }
    else if (GetType() != kGPUResourceViewType_Buffer)
    {
        vkDestroyImageView(GetVulkanDevice().GetHandle(), mImageView, nullptr);
    }
}

void VulkanResourceView::CreateImageView()
{
    auto& texture = static_cast<VulkanTexture&>(GetResource());
    Assert(texture.IsTexture());

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image                           = texture.GetHandle();
    createInfo.format                          = VulkanFormat::GetVulkanFormat(GetFormat());
    createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask     = texture.GetAspectMask();
    createInfo.subresourceRange.baseArrayLayer = GetElementOffset();
    createInfo.subresourceRange.layerCount     = GetElementCount();
    createInfo.subresourceRange.baseMipLevel   = GetMipOffset();
    createInfo.subresourceRange.levelCount     = GetMipCount();

    switch (GetType())
    {
        case kGPUResourceViewType_Texture1D:        createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;         break;
        case kGPUResourceViewType_Texture1DArray:   createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;   break;
        case kGPUResourceViewType_Texture2D:        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;         break;
        case kGPUResourceViewType_Texture2DArray:   createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;   break;
        case kGPUResourceViewType_TextureCube:      createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;       break;
        case kGPUResourceViewType_TextureCubeArray: createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; break;
        case kGPUResourceViewType_Texture3D:        createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;         break;

        default:
            UnreachableMsg("Unrecognised view type");
            break;

    }

    VulkanCheck(vkCreateImageView(GetVulkanDevice().GetHandle(),
                                  &createInfo,
                                  nullptr,
                                  &mImageView));
}
