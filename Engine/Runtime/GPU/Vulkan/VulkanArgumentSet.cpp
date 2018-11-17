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

#include "VulkanBuffer.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDevice.h"
#include "VulkanResourceView.h"
#include "VulkanTexture.h"
#include "VulkanUniformPool.h"

static VkDescriptorType kArgumentTypeMapping[kGPUArgumentTypeCount] =
{
    /* kGPUArgumentType_Uniforms        */ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
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
    mUniformOnlySet         (VK_NULL_HANDLE)
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

    if (IsUniformOnly())
    {
        mUniformOnlySet = GetVulkanDevice().GetDescriptorPool().Allocate(mHandle);

        VulkanArgumentSet::Write(mUniformOnlySet, this, nullptr);
    }
}

VulkanArgumentSetLayout::~VulkanArgumentSetLayout()
{
    if (IsUniformOnly())
    {
        GetVulkanDevice().GetDescriptorPool().Free(mUniformOnlySet);
    }

    vkDestroyDescriptorSetLayout(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}

VulkanArgumentSet::VulkanArgumentSet(VulkanDevice&               inDevice,
                                     GPUArgumentSetLayout* const inLayout,
                                     const GPUArgument* const    inArguments) :
    GPUArgumentSet  (inDevice, inLayout, inArguments)
{
    const auto layout = static_cast<const VulkanArgumentSetLayout*>(GetLayout());

    if (layout->IsUniformOnly())
    {
        mHandle = layout->GetUniformOnlySet();
    }
    else
    {
        mHandle = GetVulkanDevice().GetDescriptorPool().Allocate(layout->GetHandle());

        Write(mHandle, layout, inArguments);
    }
}

VulkanArgumentSet::~VulkanArgumentSet()
{
    if (!GetLayout()->IsUniformOnly())
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

        if (arguments[i] == kGPUArgumentType_Uniforms)
        {
            /* This just refers to the uniform pool, which we offset at bind
             * time. */
            bufferInfo.buffer = static_cast<VulkanUniformPool&>(inLayout->GetDevice().GetUniformPool()).GetHandle();
            bufferInfo.offset = 0;
            bufferInfo.range  = inLayout->GetVulkanDevice().GetLimits().maxUniformBufferRange;

            descriptorWrite.pBufferInfo = &bufferInfo;
        }
        else if (arguments[i] == kGPUArgumentType_Sampler)
        {
            Fatal("TODO");
        }
        else
        {
            const auto& view = static_cast<VulkanResourceView&>(*inArguments[i].view);

            switch (arguments[i])
            {
                case kGPUArgumentType_Buffer:
                case kGPUArgumentType_RWBuffer:
                {
                    const auto& buffer = static_cast<const VulkanBuffer&>(view.GetResource());

                    bufferInfo.buffer = buffer.GetHandle();
                    bufferInfo.offset = view.GetElementOffset();
                    bufferInfo.range  = view.GetElementCount();

                    descriptorWrite.pBufferInfo = &bufferInfo;
                    break;
                }

                case kGPUArgumentType_Texture:
                case kGPUArgumentType_RWTexture:
                {
                    // FIXME: Depth sampling layout is wrong.
                    imageInfo.imageView   = view.GetImageView();
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
                    descriptorWrite.pTexelBufferView = &view.GetBufferView();
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
