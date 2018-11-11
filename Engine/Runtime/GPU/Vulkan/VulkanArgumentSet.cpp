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

#include "VulkanArgumentSet.h"

#include "VulkanDevice.h"

VulkanArgumentSetLayout::VulkanArgumentSetLayout(VulkanDevice&              inDevice,
                                                 GPUArgumentSetLayoutDesc&& inDesc) :
    GPUArgumentSetLayout    (inDevice, std::move(inDesc)),
    mHandle                 (VK_NULL_HANDLE)
{
    const auto& arguments = GetArguments();

    std::vector<VkDescriptorSetLayoutBinding> bindings(arguments.size());

    for (size_t i = 0; i < arguments.size(); i++)
    {
        auto& binding = bindings[i];

        binding.binding            = i;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_ALL;
        binding.pImmutableSamplers = nullptr;

        switch (arguments[i])
        {
            case kGPUArgumentType_Uniforms:         binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; break;
            case kGPUArgumentType_Buffer:           binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
            case kGPUArgumentType_RWBuffer:         binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
            case kGPUArgumentType_Texture:          binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
            case kGPUArgumentType_RWTexture:        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
            case kGPUArgumentType_TextureBuffer:    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER; break;
            case kGPUArgumentType_RWTextureBuffer:  binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER; break;

            default:
                UnreachableMsg("Unhandled GPUArgumentType");
                break;

        }
    }

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings    = bindings.data();

    VulkanCheck(vkCreateDescriptorSetLayout(GetVulkanDevice().GetHandle(),
                                            &createInfo,
                                            nullptr,
                                            &mHandle));
}

VulkanArgumentSetLayout::~VulkanArgumentSetLayout()
{
    vkDestroyDescriptorSetLayout(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}
