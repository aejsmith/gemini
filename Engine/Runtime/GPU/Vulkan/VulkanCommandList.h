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

    void                            SubmitImpl(const VkCommandBuffer buffer) const;

    void                            EndImpl();
    void                            SubmitChildrenImpl(GPUCommandList** const children,
                                                       const size_t           count);

    void                            SetArgumentsImpl(const uint8_t         index,
                                                     GPUArgumentSet* const set);
    void                            SetArgumentsImpl(const uint8_t            index,
                                                     const GPUArgument* const arguments);

    void                            BindDescriptorSets(const VkPipelineBindPoint bindPoint,
                                                       const VkPipelineLayout    pipelineLayout);

private:
    T&                              GetT()          { return static_cast<T&>(*this); }
    const T&                        GetT() const    { return static_cast<const T&>(*this); }

private:
    VkCommandBuffer                 mCommandBuffer;

    /** Flattened array of completed command buffers, in submission order. */
    // FIXME: Use FrameAllocator for this.
    std::vector<VkCommandBuffer>    mCommandBuffers;

    VkDescriptorSet                 mDescriptorSets[kMaxArgumentSets];

};

class VulkanComputeCommandList final : public GPUComputeCommandList,
                                       public VulkanCommandList<VulkanComputeCommandList>
{
public:
                                    VulkanComputeCommandList(VulkanContext&                     context,
                                                             const GPUComputeCommandList* const parent);

                                    ~VulkanComputeCommandList() {}

    /**
     * GPUComputeCommandList methods.
     */
public:
    void                            Dispatch(const uint32_t groupCountX,
                                             const uint32_t groupCountY,
                                             const uint32_t groupCountZ) override;

protected:
    GPUCommandList*                 CreateChildImpl() override;

    void                            EndImpl() override
                                        { VulkanCommandList::EndImpl(); }

    void                            SubmitChildrenImpl(GPUCommandList** const children,
                                                       const size_t           count) override
                                        { VulkanCommandList::SubmitChildrenImpl(children, count); }

    void                            SetArgumentsImpl(const uint8_t         index,
                                                     GPUArgumentSet* const set) override
                                        { VulkanCommandList::SetArgumentsImpl(index, set); }

    void                            SetArgumentsImpl(const uint8_t            index,
                                                     const GPUArgument* const arguments) override
                                        { VulkanCommandList::SetArgumentsImpl(index, arguments); }

    /**
     * Internal methods.
     */
public:
    void                            BeginCommandBuffer(const VkCommandBuffer buffer) const;

    void                            Submit(const VkCommandBuffer buffer) const;

private:
    void                            PreDispatch();

    friend class VulkanCommandList<VulkanComputeCommandList>;

};

class VulkanGraphicsCommandList final : public GPUGraphicsCommandList,
                                        public VulkanCommandList<VulkanGraphicsCommandList>
{
public:
                                    VulkanGraphicsCommandList(VulkanContext&                      context,
                                                              const GPUGraphicsCommandList* const parent,
                                                              const GPURenderPass&                renderPass);

                                    ~VulkanGraphicsCommandList() {}

    /**
     * GPUGraphicsCommandList methods.
     */
public:
    void                            Draw(const uint32_t vertexCount,
                                         const uint32_t firstVertex) override;

    void                            DrawIndexed(const uint32_t indexCount,
                                                const uint32_t firstIndex,
                                                const int32_t  vertexOffset) override;

protected:
    GPUCommandList*                 CreateChildImpl() override;

    void                            EndImpl() override
                                        { VulkanCommandList::EndImpl(); }

    void                            SubmitChildrenImpl(GPUCommandList** const children,
                                                       const size_t           count) override
                                        { VulkanCommandList::SubmitChildrenImpl(children, count); }

    void                            SetArgumentsImpl(const uint8_t         index,
                                                     GPUArgumentSet* const set) override
                                        { VulkanCommandList::SetArgumentsImpl(index, set); }

    void                            SetArgumentsImpl(const uint8_t            index,
                                                     const GPUArgument* const arguments) override
                                        { VulkanCommandList::SetArgumentsImpl(index, arguments); }

    uint32_t                        AllocateTransientBuffer(const size_t size,
                                                            void*&       outMapping) override;

    /**
     * Internal methods.
     */
public:
    VkRenderPass                    GetVulkanRenderPass() const     { return mVulkanRenderPass; }
    VkFramebuffer                   GetFramebuffer() const          { return mFramebuffer; }

    void                            BeginCommandBuffer(const VkCommandBuffer buffer) const;

    void                            Submit(const VkCommandBuffer buffer) const;

private:
    void                            PreDraw(const bool isIndexed);

private:
    VkRenderPass                    mVulkanRenderPass;
    VkFramebuffer                   mFramebuffer;

    friend class VulkanCommandList<VulkanGraphicsCommandList>;

};
