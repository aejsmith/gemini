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
#include "VulkanConstantPool.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDevice.h"
#include "VulkanResourceView.h"
#include "VulkanSampler.h"
#include "VulkanTexture.h"

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

VulkanArgumentSetLayout::VulkanArgumentSetLayout(VulkanDevice&              inDevice,
                                                 GPUArgumentSetLayoutDesc&& inDesc) :
    GPUArgumentSetLayout    (inDevice, std::move(inDesc)),
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

VulkanArgumentSet::VulkanArgumentSet(VulkanDevice&                 inDevice,
                                     const GPUArgumentSetLayoutRef inLayout,
                                     const GPUArgument* const      inArguments) :
    GPUArgumentSet  (inDevice, inLayout, inArguments)
{
    const auto layout = static_cast<const VulkanArgumentSetLayout*>(GetLayout());

    if (layout->IsConstantOnly())
    {
        mHandle = layout->GetConstantOnlySet();
    }
    else
    {
        mHandle = GetVulkanDevice().GetDescriptorPool().Allocate(layout->GetHandle());

        Write(mHandle, layout, inArguments);
    }
}

VulkanArgumentSet::~VulkanArgumentSet()
{
    if (!GetLayout()->IsConstantOnly())
    {
        GetVulkanDevice().AddFrameCompleteCallback(
            [handle = mHandle] (VulkanDevice& inDevice)
            {
                inDevice.GetDescriptorPool().Free(handle);
            });
    }
}

void VulkanArgumentSet::Write(const VkDescriptorSet                inHandle,
                              const VulkanArgumentSetLayout* const inLayout,
                              const GPUArgument* const             inArguments)
{
    const auto& arguments = inLayout->GetArguments();

    auto descriptorWrites = AllocateStackArray(VkWriteDescriptorSet, inLayout->GetArgumentCount());
    auto bufferInfos      = AllocateStackArray(VkDescriptorBufferInfo, inLayout->GetArgumentCount());
    auto imageInfos       = AllocateStackArray(VkDescriptorImageInfo, inLayout->GetArgumentCount());

    for (uint8_t i = 0; i < inLayout->GetArgumentCount(); i++)
    {
        auto& descriptorWrite = descriptorWrites[i];
        auto& bufferInfo      = bufferInfos[i];
        auto& imageInfo       = imageInfos[i];

        descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.pNext            = nullptr;
        descriptorWrite.dstSet           = inHandle;
        descriptorWrite.dstBinding       = i;
        descriptorWrite.dstArrayElement  = 0;
        descriptorWrite.descriptorCount  = 1;
        descriptorWrite.descriptorType   = kArgumentTypeMapping[arguments[i]];
        descriptorWrite.pImageInfo       = nullptr;
        descriptorWrite.pBufferInfo      = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        if (arguments[i] == kGPUArgumentType_Constants)
        {
            /* This just refers to the constant pool, which we offset at bind
             * time. */
            bufferInfo.buffer = static_cast<VulkanConstantPool&>(inLayout->GetDevice().GetConstantPool()).GetHandle();
            bufferInfo.offset = 0;
            bufferInfo.range  = std::min(inLayout->GetVulkanDevice().GetLimits().maxUniformBufferRange, kMaxConstantsSize);

            descriptorWrite.pBufferInfo = &bufferInfo;
        }
        else if (arguments[i] == kGPUArgumentType_Sampler)
        {
            const auto sampler = static_cast<const VulkanSampler*>(inArguments[i].sampler);

            imageInfo.imageView   = VK_NULL_HANDLE;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.sampler     = sampler->GetHandle();

            descriptorWrite.pImageInfo = &imageInfo;
        }
        else
        {
            const auto view = static_cast<VulkanResourceView*>(inArguments[i].view);

            switch (arguments[i])
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
                    imageInfo.imageLayout = (arguments[i] == kGPUArgumentType_RWTexture)
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

    vkUpdateDescriptorSets(inLayout->GetVulkanDevice().GetHandle(),
                           inLayout->GetArgumentCount(),
                           descriptorWrites,
                           0,
                           nullptr);
}
