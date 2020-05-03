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

#include "VulkanArgumentSet.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDevice.h"
#include "VulkanResourceView.h"
#include "VulkanSampler.h"
#include "VulkanTexture.h"
#include "VulkanTransientPool.h"

static VkDescriptorType kArgumentTypeMapping[kGPUArgumentTypeCount] =
{
    /* kGPUArgumentType_Constants       */ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    /* kGPUArgumentType_Buffer          */ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    /* kGPUArgumentType_RWBuffer        */ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    /* kGPUArgumentType_Texture         */ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    /* kGPUArgumentType_RWTexture       */ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    /* kGPUArgumentType_TextureBuffer   */ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
    /* kGPUArgumentType_RWTextureBuffer */ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
    /* kGPUArgumentType_Sampler         */ VK_DESCRIPTOR_TYPE_SAMPLER,
};

VulkanArgumentSetLayout::VulkanArgumentSetLayout(VulkanDevice&              device,
                                                 GPUArgumentSetLayoutDesc&& desc) :
    GPUArgumentSetLayout    (device, std::move(desc)),
    mHandle                 (VK_NULL_HANDLE),
    mConstantOnlySet        (VK_NULL_HANDLE)
{
    const auto& arguments = GetArguments();

    std::vector<VkDescriptorSetLayoutBinding> bindings(GetArgumentCount());

    for (size_t i = 0; i < GetArgumentCount(); i++)
    {
        auto& binding = bindings[i];

        binding.binding            = i;
        binding.descriptorCount    = 1;
        binding.descriptorType     = kArgumentTypeMapping[arguments[i]];
        binding.stageFlags         = VK_SHADER_STAGE_ALL;
        binding.pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings    = bindings.data();

    VulkanCheck(vkCreateDescriptorSetLayout(GetVulkanDevice().GetHandle(),
                                            &createInfo,
                                            nullptr,
                                            &mHandle));

    if (IsConstantOnly())
    {
        mConstantOnlySet = GetVulkanDevice().GetDescriptorPool().Allocate(mHandle);

        VulkanArgumentSet::Write(mConstantOnlySet, this, nullptr);
    }
}

VulkanArgumentSetLayout::~VulkanArgumentSetLayout()
{
    if (IsConstantOnly())
    {
        GetVulkanDevice().GetDescriptorPool().Free(mConstantOnlySet);
    }

    vkDestroyDescriptorSetLayout(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}

VulkanArgumentSet::VulkanArgumentSet(VulkanDevice&                 device,
                                     const GPUArgumentSetLayoutRef layout,
                                     const GPUArgument* const      arguments) :
    GPUArgumentSet  (device, layout, arguments)
{
    const auto vkLayout = static_cast<const VulkanArgumentSetLayout*>(GetLayout());

    if (vkLayout->IsConstantOnly())
    {
        mHandle = vkLayout->GetConstantOnlySet();
    }
    else
    {
        mHandle = GetVulkanDevice().GetDescriptorPool().Allocate(vkLayout->GetHandle());

        Write(mHandle, vkLayout, arguments);
    }
}

VulkanArgumentSet::~VulkanArgumentSet()
{
    if (!GetLayout()->IsConstantOnly())
    {
        GetVulkanDevice().AddFrameCompleteCallback(
            [handle = mHandle] (VulkanDevice& device)
            {
                device.GetDescriptorPool().Free(handle);
            });
    }
}

void VulkanArgumentSet::Write(const VkDescriptorSet                handle,
                              const VulkanArgumentSetLayout* const layout,
                              const GPUArgument* const             arguments)
{
    const auto& argumentTypes = layout->GetArguments();

    auto descriptorWrites = AllocateStackArray(VkWriteDescriptorSet, layout->GetArgumentCount());
    auto bufferInfos      = AllocateStackArray(VkDescriptorBufferInfo, layout->GetArgumentCount());
    auto imageInfos       = AllocateStackArray(VkDescriptorImageInfo, layout->GetArgumentCount());

    for (uint8_t i = 0; i < layout->GetArgumentCount(); i++)
    {
        auto& descriptorWrite = descriptorWrites[i];
        auto& bufferInfo      = bufferInfos[i];
        auto& imageInfo       = imageInfos[i];

        descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.pNext            = nullptr;
        descriptorWrite.dstSet           = handle;
        descriptorWrite.dstBinding       = i;
        descriptorWrite.dstArrayElement  = 0;
        descriptorWrite.descriptorCount  = 1;
        descriptorWrite.descriptorType   = kArgumentTypeMapping[argumentTypes[i]];
        descriptorWrite.pImageInfo       = nullptr;
        descriptorWrite.pBufferInfo      = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        if (argumentTypes[i] == kGPUArgumentType_Constants)
        {
            /* This just refers to the constant pool, which we offset at bind
             * time. */
            bufferInfo.buffer = static_cast<VulkanConstantPool&>(layout->GetDevice().GetConstantPool()).GetHandle();
            bufferInfo.offset = 0;
            bufferInfo.range  = std::min(layout->GetVulkanDevice().GetLimits().maxUniformBufferRange, kMaxConstantsSize);

            descriptorWrite.pBufferInfo = &bufferInfo;
        }
        else if (argumentTypes[i] == kGPUArgumentType_Sampler)
        {
            const auto sampler = static_cast<const VulkanSampler*>(arguments[i].sampler);

            imageInfo.imageView   = VK_NULL_HANDLE;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.sampler     = sampler->GetHandle();

            descriptorWrite.pImageInfo = &imageInfo;
        }
        else
        {
            const auto view = static_cast<VulkanResourceView*>(arguments[i].view);

            switch (argumentTypes[i])
            {
                case kGPUArgumentType_Buffer:
                case kGPUArgumentType_RWBuffer:
                {
                    const auto& buffer = static_cast<const VulkanBuffer&>(view->GetResource());

                    bufferInfo.buffer = buffer.GetHandle();
                    bufferInfo.offset = view->GetElementOffset();
                    bufferInfo.range  = view->GetElementCount();

                    descriptorWrite.pBufferInfo = &bufferInfo;
                    break;
                }

                case kGPUArgumentType_Texture:
                case kGPUArgumentType_RWTexture:
                {
                    // FIXME: Depth sampling layout is wrong.
                    imageInfo.imageView   = view->GetImageView();
                    imageInfo.imageLayout = (argumentTypes[i] == kGPUArgumentType_RWTexture)
                                                ? VK_IMAGE_LAYOUT_GENERAL
                                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.sampler     = VK_NULL_HANDLE;

                    descriptorWrite.pImageInfo = &imageInfo;

                    break;
                }

                case kGPUArgumentType_TextureBuffer:
                case kGPUArgumentType_RWTextureBuffer:
                {
                    /* Returns by reference. */
                    descriptorWrite.pTexelBufferView = &view->GetBufferView();
                    break;
                }

                default:
                    UnreachableMsg("Unknown GPUArgumentType");

            }
        }
    }

    vkUpdateDescriptorSets(layout->GetVulkanDevice().GetHandle(),
                           layout->GetArgumentCount(),
                           descriptorWrites,
                           0,
                           nullptr);
}
