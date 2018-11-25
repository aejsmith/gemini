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

#include "VulkanSampler.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"

VulkanSampler::VulkanSampler(VulkanDevice&         inDevice,
                             const GPUSamplerDesc& inDesc) :
    GPUSampler  (inDevice)
{
    VkSamplerCreateInfo createInfo = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter        = VulkanUtils::ConvertFilter(inDesc.magFilter);
    createInfo.minFilter        = VulkanUtils::ConvertFilter(inDesc.minFilter);
    createInfo.mipmapMode       = VulkanUtils::ConvertMipmapMode(inDesc.mipmapFilter);
    createInfo.addressModeU     = VulkanUtils::ConvertAddressMode(inDesc.addressU);
    createInfo.addressModeV     = VulkanUtils::ConvertAddressMode(inDesc.addressV);
    createInfo.addressModeW     = VulkanUtils::ConvertAddressMode(inDesc.addressW);
    createInfo.mipLodBias       = inDesc.lodBias;
    createInfo.anisotropyEnable = inDesc.maxAnisotropy > 0;
    createInfo.maxAnisotropy    = inDesc.maxAnisotropy;
    createInfo.compareEnable    = inDesc.compareOp != kGPUCompareOp_Always;
    createInfo.compareOp        = VulkanUtils::ConvertCompareOp(inDesc.compareOp);
    createInfo.minLod           = inDesc.minLod;
    createInfo.maxLod           = inDesc.maxLod;

    VulkanCheck(vkCreateSampler(GetVulkanDevice().GetHandle(),
                                &createInfo,
                                nullptr,
                                &mHandle));
}

VulkanSampler::~VulkanSampler()
{
    /* Samplers are cached and destroyed at device destruction, no need to
     * defer. */
    vkDestroySampler(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}
