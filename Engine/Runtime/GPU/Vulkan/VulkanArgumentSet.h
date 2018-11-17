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

#include "GPU/GPUArgumentSet.h"

#include "VulkanDeviceChild.h"

class VulkanArgumentSetLayout final : public GPUArgumentSetLayout,
                                      public VulkanDeviceChild<VulkanArgumentSetLayout>
{
public:
                            VulkanArgumentSetLayout(VulkanDevice&              inDevice,
                                                    GPUArgumentSetLayoutDesc&& inDesc);

                            ~VulkanArgumentSetLayout();

    VkDescriptorSetLayout   GetHandle() const           { return mHandle; }

    /**
     * When a layout only contains uniforms arguments, we can create a single
     * set up front with the layout, and always re-use this instead of creating
     * any other sets, since we just need to bind it with the appropriate
     * offset for the bound uniform handles.
     */
    VkDescriptorSet         GetUniformOnlySet() const   { return mUniformOnlySet; }

private:
    VkDescriptorSetLayout   mHandle;

    VkDescriptorSet         mUniformOnlySet;

};

class VulkanArgumentSet final : public GPUArgumentSet,
                                public VulkanDeviceChild<VulkanArgumentSetLayout>
{
public:
                            VulkanArgumentSet(VulkanDevice&               inDevice,
                                              GPUArgumentSetLayout* const inLayout,
                                              const GPUArgument* const    inArguments);

                            ~VulkanArgumentSet();

    VkDescriptorSet         GetHandle() const   { return mHandle; }

    static void             Write(const VkDescriptorSet                inHandle,
                                  const VulkanArgumentSetLayout* const inLayout,
                                  const GPUArgument* const             inArguments);

private:
    VkDescriptorSet         mHandle;

};
