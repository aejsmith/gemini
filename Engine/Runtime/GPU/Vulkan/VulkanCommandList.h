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

    void                            SubmitImpl(const VkCommandBuffer inBuffer) const;

    void                            EndImpl();
    void                            SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                       const size_t           inCount);

    void                            SetArgumentsImpl(const uint8_t         inIndex,
                                                     GPUArgumentSet* const inSet);
    void                            SetArgumentsImpl(const uint8_t            inIndex,
                                                     const GPUArgument* const inArguments);

    void                            BindDescriptorSets(const VkPipelineBindPoint inBindPoint,
                                                       const VkPipelineLayout    inPipelineLayout);

private:
    T&                              GetT()          { return static_cast<T&>(*this); }
    const T&                        GetT() const    { return static_cast<const T&>(*this); }

private:
    VkCommandBuffer                 mCommandBuffer;

    /** Flattened array of completed command buffers, in submission order. */
    std::vector<VkCommandBuffer>    mCommandBuffers;

    VkDescriptorSet                 mDescriptorSets[kMaxArgumentSets];

};

class VulkanComputeCommandList final : public GPUComputeCommandList,
                                       public VulkanCommandList<VulkanComputeCommandList>
{
public:
                                    VulkanComputeCommandList(VulkanContext&                     inContext,
                                                             const GPUComputeCommandList* const inParent);

                                    ~VulkanComputeCommandList() {}

    /**
     * GPUComputeCommandList methods.
     */
public:
    void                            Dispatch(const uint32_t inGroupCountX,
                                             const uint32_t inGroupCountY,
                                             const uint32_t inGroupCountZ) override;

protected:
    GPUCommandList*                 CreateChildImpl() override;

    void                            EndImpl() override
                                        { VulkanCommandList::EndImpl(); }

    void                            SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                       const size_t           inCount) override
                                        { VulkanCommandList::SubmitChildrenImpl(inChildren, inCount); }

    void                            SetArgumentsImpl(const uint8_t         inIndex,
                                                     GPUArgumentSet* const inSet) override
                                        { VulkanCommandList::SetArgumentsImpl(inIndex, inSet); }

    void                            SetArgumentsImpl(const uint8_t            inIndex,
                                                     const GPUArgument* const inArguments) override
                                        { VulkanCommandList::SetArgumentsImpl(inIndex, inArguments); }

    /**
     * Internal methods.
     */
public:
    void                            BeginCommandBuffer(const VkCommandBuffer inBuffer) const;

    void                            Submit(const VkCommandBuffer inBuffer) const;

private:
    void                            PreDispatch();

    friend class VulkanCommandList<VulkanComputeCommandList>;

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
    void                            Draw(const uint32_t inVertexCount,
                                         const uint32_t inFirstVertex) override;

    void                            DrawIndexed(const uint32_t inIndexCount,
                                                const uint32_t inFirstIndex,
                                                const int32_t  inVertexOffset) override;

protected:
    GPUCommandList*                 CreateChildImpl() override;

    void                            EndImpl() override
                                        { VulkanCommandList::EndImpl(); }

    void                            SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                       const size_t           inCount) override
                                        { VulkanCommandList::SubmitChildrenImpl(inChildren, inCount); }

    void                            SetArgumentsImpl(const uint8_t         inIndex,
                                                     GPUArgumentSet* const inSet) override
                                        { VulkanCommandList::SetArgumentsImpl(inIndex, inSet); }

    void                            SetArgumentsImpl(const uint8_t            inIndex,
                                                     const GPUArgument* const inArguments) override
                                        { VulkanCommandList::SetArgumentsImpl(inIndex, inArguments); }

    /**
     * Internal methods.
     */
public:
    VkRenderPass                    GetVulkanRenderPass() const     { return mVulkanRenderPass; }
    VkFramebuffer                   GetFramebuffer() const          { return mFramebuffer; }

    void                            BeginCommandBuffer(const VkCommandBuffer inBuffer) const;

    void                            Submit(const VkCommandBuffer inBuffer) const;

private:
    void                            PreDraw(const bool inIsIndexed);

private:
    VkRenderPass                    mVulkanRenderPass;
    VkFramebuffer                   mFramebuffer;

    friend class VulkanCommandList<VulkanGraphicsCommandList>;

};
