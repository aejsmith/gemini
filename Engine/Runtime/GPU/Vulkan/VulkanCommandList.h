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

#include "GPU/GPUCommandList.h"

#include "VulkanDeviceChild.h"

#include <vector>

class VulkanContext;

/** Shared implementation details between Vulkan command list classes. */
template <typename T>
class VulkanCommandList : public VulkanDeviceChild<T>
{
protected:
                                    VulkanCommandList();

protected:
    VulkanContext&                  GetVulkanContext() const;

    /**
     * Get the handle for the current command buffer. If one is not currently
     * in progress, a new one will be allocated and begun.
     */
    VkCommandBuffer                 GetCommandBuffer();

    void                            SubmitImpl(const VkCommandBuffer inBuffer) const;

    void                            EndImpl();
    void                            SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                       const size_t           inCount);

private:
    T&                              GetT()          { return static_cast<T&>(*this); }
    const T&                        GetT() const    { return static_cast<const T&>(*this); }

private:
    VkCommandBuffer                 mCommandBuffer;

    /** Flattened array of completed command buffers, in submission order. */
    std::vector<VkCommandBuffer>    mCommandBuffers;

};

class VulkanGraphicsCommandList final : public GPUGraphicsCommandList,
                                        public VulkanCommandList<VulkanGraphicsCommandList>
{
public:
                                    VulkanGraphicsCommandList(VulkanContext&                      inContext,
                                                              const GPUGraphicsCommandList* const inParent,
                                                              const GPURenderPass&                inRenderPass);

                                    ~VulkanGraphicsCommandList() {}

    /**
     * GPUGraphicsCommandList methods.
     */
public:
    void                            SetPipeline(GPUPipeline* const inPipeline) override;

protected:
    GPUCommandList*                 CreateChildImpl() override;

    void                            EndImpl() override
                                        { VulkanCommandList::EndImpl(); }

    void                            SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                       const size_t           inCount) override
                                        { VulkanCommandList::SubmitChildrenImpl(inChildren, inCount); }

    /**
     * Internal methods.
     */
public:
    VkRenderPass                    GetVulkanRenderPass() const     { return mVulkanRenderPass; }
    VkFramebuffer                   GetFramebuffer() const          { return mFramebuffer; }

    void                            BeginCommandBuffer(const VkCommandBuffer inBuffer) const;

    void                            Submit(const VkCommandBuffer inBuffer) const;

private:
    VkRenderPass                    mVulkanRenderPass;
    VkFramebuffer                   mFramebuffer;

};
