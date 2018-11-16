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

#include "GPU/GPUUniformPool.h"

#include "VulkanMemoryManager.h"

#include <atomic>

/**
 * Vulkan implementation of GPUUniformPool. This just uses a single VkBuffer
 * divided up between each in-flight frame. GPUUniforms handles are just an
 * offset into that buffer. This means that we can always create descriptors
 * for kGPUArgumentType_Uniforms arguments as UNIFORM_BUFFER_DYNAMIC referring
 * to this VkBuffer, and then just plug in the offset at bind time.
 */
class VulkanUniformPool final : public GPUUniformPool,
                                public VulkanDeviceChild<VulkanUniformPool>
{
public:
                            VulkanUniformPool(VulkanDevice& inDevice);
                            ~VulkanUniformPool();

    VkBuffer                GetHandle() const   { return mHandle; }

    GPUUniforms             Allocate(const size_t inSize,
                                     void*&       outMapping) override;

    void                    BeginFrame();

private:
    VkBuffer                mHandle;
    VmaAllocation           mAllocation;
    uint8_t*                mMapping;
    const uint32_t          mAlignment;
    std::atomic<uint32_t>   mCurrentOffset;

};
