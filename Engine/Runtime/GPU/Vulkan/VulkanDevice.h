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

#include "GPU/GPUDevice.h"

#include "VulkanInstance.h"
#include "VulkanRenderPass.h"

#include "Core/HashTable.h"

#include <functional>
#include <list>
#include <vector>

class VulkanContext;
class VulkanMemoryManager;

class VulkanDevice final : public GPUDevice
{
public:
                                        VulkanDevice();
                                        ~VulkanDevice();

    /**
     * GPUDevice methods.
     */
public:
    GPUBufferPtr                        CreateBuffer(const GPUBufferDesc& inDesc) override;

    GPUResourceViewPtr                  CreateResourceView(GPUResource&               inResource,
                                                           const GPUResourceViewDesc& inDesc) override;

    GPUShaderPtr                        CreateShader(const GPUShaderStage inStage,
                                                     GPUShaderCode        inCode) override;

    void                                CreateSwapchain(Window& inWindow) override;

    GPUTexturePtr                       CreateTexture(const GPUTextureDesc& inDesc) override;

protected:
    GPUPipeline*                        CreatePipelineImpl(const GPUPipelineDesc& inDesc) override;

    void                                EndFrameImpl() override;

    /**
     * Internal methods.
     */
public:
    using FrameCompleteCallback       = std::function<void (VulkanDevice&)>;
    using FrameCompleteCallbackList   = std::list<FrameCompleteCallback>;

public:
    VulkanInstance&                     GetInstance() const             { return *mInstance; }

    VkPhysicalDevice                    GetPhysicalDevice() const       { return mPhysicalDevice; }
    VkDevice                            GetHandle() const               { return mHandle; }
    uint32_t                            GetGraphicsQueueFamily() const  { return mGraphicsQueueFamily; }
    const VkPhysicalDeviceProperties&   GetProperties() const           { return mProperties; }
    const VkPhysicalDeviceLimits&       GetLimits() const               { return mProperties.limits; }
    const VkPhysicalDeviceFeatures&     GetFeatures() const             { return mFeatures; }
    VkPipelineCache                     GetDriverPipelineCache() const  { return mDriverPipelineCache; }

    VulkanMemoryManager&                GetMemoryManager() const        { return *mMemoryManager; }

    /**
     * Get the current frame index (between 0 and kVulkanInFlightFrameCount),
     * for indexing data tracked for in-flight frames.
     */
    uint8_t                             GetCurrentFrame() const         { return mCurrentFrame; }

    /**
     * Add a function to be called when the current frame has completed on the
     * GPU. This can be used for deferred deletion.
     */
    void                                AddFrameCompleteCallback(FrameCompleteCallback inCallback);

    /**
     * Get a semaphore. This should be used within the current frame - once it
     * has completed on the GPU, it will be returned to the free pool to be
     * reused. If the semaphore is signalled within the frame, it must also
     * be waited on - this is required to unsignal the semaphore so that it can
     * be reused.
     */
    VkSemaphore                         AllocateSemaphore();

    /**
     * Get a fence for a submission. This should be used within the current
     * frame - all fences allocated with this function will be waited on when
     * waiting for the frame to complete.
     */
    VkFence                             AllocateFence();

    /**
     * Get a Vulkan render pass and framebuffer object matching the given
     * render pass description from a cache. If no matching objects are found,
     * new ones will be created. Must be called from the main thread.
     */
    void                                GetRenderPass(const GPURenderPass& inRenderPass,
                                                      VkRenderPass&        outVulkanRenderPass,
                                                      VkFramebuffer&       outFramebuffer);

    /**
     * Get a Vulkan render pass matching the given render target state, which
     * should be compatible with any real render pass matching the state.
     */
    VkRenderPass                        GetRenderPass(const GPURenderTargetStateDesc& inState);

    /**
     * Callback from VulkanResourceView and VulkanSwapchain to invalidate any
     * framebuffers referring to a view.
     */
    void                                InvalidateFramebuffers(const VkImageView inView);

private:
    struct Frame
    {
        FrameCompleteCallbackList       completeCallbacks;

        /**
         * Semaphores used in the frame. Returned to the pool once the frame
         * is completed.
         */
        std::vector<VkSemaphore>        semaphores;

        /**
         * Fences for every submission in the frame. Used to determine when
         * the frame is completed. Returned to the pool once completed.
         */
        std::vector<VkFence>            fences;
    };

    using RenderPassCache             = HashMap<VulkanRenderPassKey, VkRenderPass>;
    using FramebufferCache            = HashMap<VulkanFramebufferKey, VkFramebuffer>;

private:
    void                                CreateDevice();

    VkRenderPass                        GetRenderPass(const VulkanRenderPassKey& inKey);

private:
    VulkanInstance*                     mInstance;

    VkPhysicalDevice                    mPhysicalDevice;
    VkDevice                            mHandle;
    uint32_t                            mGraphicsQueueFamily;
    VkPhysicalDeviceProperties          mProperties;
    VkPhysicalDeviceFeatures            mFeatures;
    VkPipelineCache                     mDriverPipelineCache;

    VulkanMemoryManager*                mMemoryManager;

    /**
     * Per-frame data. Indexed by mCurrentFrame, which is a value between 0
     * and kVulkanInFlightFrameCount.
     */
    uint8_t                             mCurrentFrame;
    Frame                               mFrames[kVulkanInFlightFrameCount];

    VulkanContext*                      mContexts[kVulkanMaxContexts];

    std::vector<VkSemaphore>            mSemaphorePool;
    std::vector<VkFence>                mFencePool;

    RenderPassCache                     mRenderPassCache;
    FramebufferCache                    mFramebufferCache;

};
