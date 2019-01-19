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

#include "VulkanTexture.h"

#include "VulkanDevice.h"
#include "VulkanFormat.h"
#include "VulkanSwapchain.h"

VulkanTexture::VulkanTexture(VulkanDevice&         inDevice,
                             const GPUTextureDesc& inDesc) :
    GPUTexture  (inDevice, inDesc),
    mHandle     (VK_NULL_HANDLE),
    mAllocation (VK_NULL_HANDLE),
    mAspectMask (0)
{
    if (PixelFormat::IsDepth(GetFormat()))
    {
        mAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (PixelFormat::IsDepthStencil(GetFormat()))
        {
            mAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        mAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageCreateInfo createInfo = {};
    createInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.format        = VulkanFormat::GetVulkanFormat(GetFormat());
    createInfo.extent.width  = GetWidth();
    createInfo.extent.height = GetHeight();
    createInfo.extent.depth  = GetDepth();
    createInfo.mipLevels     = GetNumMipLevels();
    createInfo.arrayLayers   = GetArraySize();
    createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (IsCubeCompatible()) createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    switch (GetType())
    {
        case kGPUResourceType_Texture1D: createInfo.imageType = VK_IMAGE_TYPE_1D; break;
        case kGPUResourceType_Texture2D: createInfo.imageType = VK_IMAGE_TYPE_2D; break;
        case kGPUResourceType_Texture3D: createInfo.imageType = VK_IMAGE_TYPE_3D; break;

        default:
            UnreachableMsg("Unrecognised texture type");
            break;

    }

    /* We always want to be able to transfer to and from images. */
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (GetUsage() & kGPUResourceUsage_ShaderRead)   createInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (GetUsage() & kGPUResourceUsage_ShaderWrite)  createInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (GetUsage() & kGPUResourceUsage_RenderTarget) createInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (GetUsage() & kGPUResourceUsage_DepthStencil) createInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.pUserData = this;
    allocationInfo.usage     = VMA_MEMORY_USAGE_GPU_ONLY;

    if (GetUsage() & (kGPUResourceUsage_RenderTarget | kGPUResourceUsage_DepthStencil))
    {
        /*
         * Mark render target allocations as dedicated for NVIDIA. This provides
         * a significant performance boost for some cards.
         */
        if (GetVulkanDevice().GetVendor() == kGPUVendor_NVIDIA)
        {
            allocationInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }
    }

    if (GetUsage() & (kGPUResourceUsage_RenderTarget | kGPUResourceUsage_DepthStencil | kGPUResourceUsage_ShaderWrite))
    {
        /*
         * Don't allow render target or shader writable texture allocations in
         * host memory. This will perform terribly.
         */
        allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    GetVulkanDevice().GetMemoryManager().AllocateImage(createInfo,
                                                       allocationInfo,
                                                       mHandle,
                                                       mAllocation);
}

VulkanTexture::VulkanTexture(VulkanSwapchain& inSwapchain,
                             OnlyCalledBy<VulkanSwapchain>) :
    GPUTexture  (inSwapchain),
    mHandle     (VK_NULL_HANDLE),
    mAllocation (VK_NULL_HANDLE),
    mAspectMask (VK_IMAGE_ASPECT_COLOR_BIT)
{
}

VulkanTexture::~VulkanTexture()
{
    if (!IsSwapchain())
    {
        GetVulkanDevice().AddFrameCompleteCallback(
            [handle = mHandle, allocation = mAllocation] (VulkanDevice& inDevice)
            {
                vkDestroyImage(inDevice.GetHandle(), handle, nullptr);
                inDevice.GetMemoryManager().Free(allocation);
            });
    }
}

void VulkanTexture::UpdateName()
{
    GetVulkanDevice().UpdateName(mHandle,
                                 VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT,
                                 GetName());
}
