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

#include "GPU/GPUPipeline.h"

#include "VulkanDeviceChild.h"

class VulkanPipeline final : public GPUPipeline,
                             public VulkanDeviceChild<VulkanPipeline>
{
public:
                            VulkanPipeline(VulkanDevice&          device,
                                           const GPUPipelineDesc& desc);

                            ~VulkanPipeline();

    VkPipeline              GetHandle() const       { return mHandle; }
    VkPipelineLayout        GetLayout() const       { return mLayout; }

    uint32_t                GetDummyVertexBuffer() const  { return mDummyVertexBuffer; }
    bool                    NeedsDummyVertexBuffer() const
                                { return mDummyVertexBuffer != kMaxVertexAttributes; }

private:
    VkPipeline              mHandle;
    VkPipelineLayout        mLayout;

    uint32_t                mDummyVertexBuffer;

};

class VulkanComputePipeline final : public GPUComputePipeline,
                                    public VulkanDeviceChild<VulkanComputePipeline>
{
public:
                            VulkanComputePipeline(VulkanDevice&                 device,
                                                  const GPUComputePipelineDesc& desc);

                            ~VulkanComputePipeline();

    VkPipeline              GetHandle() const { return mHandle; }
    VkPipelineLayout        GetLayout() const { return mLayout; }

private:
    VkPipeline              mHandle;
    VkPipelineLayout        mLayout;

};
