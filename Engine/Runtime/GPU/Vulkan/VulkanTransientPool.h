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

#include "GPU/GPUConstantPool.h"

#include "VulkanMemoryManager.h"

#include <atomic>

/**
 * Vulkan transient memory pool base class. This just uses a single VkBuffer
 * divided up between each in-flight frame. For constants, GPUConstants handles
 * are just an offset into that buffer. This means that we can always create
 * descriptors for kGPUArgumentType_Constants arguments as
 * UNIFORM_BUFFER_DYNAMIC referring to this VkBuffer, and then just plug in the
 * offset at bind time.
 */
class VulkanTransientPool
{
public:
                            VulkanTransientPool(VulkanDevice&            device,
                                                const VkBufferUsageFlags usageFlags,
                                                const uint32_t           perFramePoolSize,
                                                const uint32_t           alignment,
                                                const char* const        name);

                            ~VulkanTransientPool();

    VkBuffer                GetHandle() const   { return mHandle; }

    uint32_t                Allocate(const size_t size,
                                     void*&       outMapping);

    void                    BeginFrame();

private:
    VulkanDevice&           mDevice;

    const uint32_t          mPerFramePoolSize;
    const uint32_t          mAlignment;

    VkBuffer                mHandle;
    VmaAllocation           mAllocation;
    uint8_t*                mMapping;
    std::atomic<uint32_t>   mCurrentOffset;

};

class VulkanConstantPool final : public GPUConstantPool,
                                 public VulkanTransientPool
{
public:
                            VulkanConstantPool(VulkanDevice& device);
                            ~VulkanConstantPool();

    GPUConstants            Allocate(const size_t size,
                                     void*&       outMapping) override
                                { return VulkanTransientPool::Allocate(size, outMapping); }

};

class VulkanGeometryPool final : public VulkanTransientPool
{
public:
                            VulkanGeometryPool(VulkanDevice& device);
                            ~VulkanGeometryPool();
};
