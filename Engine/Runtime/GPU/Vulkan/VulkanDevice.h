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

#include "GPU/GPUDevice.h"

#include "VulkanInstance.h"
#include "VulkanPipelineLayoutKey.h"
#include "VulkanRenderPass.h"

#include "Core/HashTable.h"

#include <functional>
#include <list>
#include <vector>

class VulkanArgumentSetLayout;
class VulkanContext;
class VulkanDescriptorPool;
class VulkanGeometryPool;
class VulkanMemoryManager;

class VulkanDevice final : public GPUDevice
{
public:
    /** Device capability flags. */
    enum Caps : uint32_t
    {
        kCap_DebugMarker        = (1 << 0),
    };

public:
                                        VulkanDevice();
                                        ~VulkanDevice();

    /**
     * GPUDevice methods.
     */
public:
    GPUArgumentSet*                     CreateArgumentSet(const GPUArgumentSetLayoutRef layout,
                                                          const GPUArgument* const      arguments) override;

    GPUBuffer*                          CreateBuffer(const GPUBufferDesc& desc) override;

    GPUComputePipeline*                 CreateComputePipeline(const GPUComputePipelineDesc& desc) override;

    GPUResourceView*                    CreateResourceView(GPUResource* const         resource,
                                                           const GPUResourceViewDesc& desc) override;

    GPUShaderPtr                        CreateShader(const GPUShaderStage stage,
                                                     GPUShaderCode        code,
                                                     const std::string&   function) override;

    void                                CreateSwapchain(Window& window) override;

    GPUTexture*                         CreateTexture(const GPUTextureDesc& desc) override;

protected:
    GPUArgumentSetLayout*               CreateArgumentSetLayoutImpl(GPUArgumentSetLayoutDesc&& desc) override;
    GPUPipeline*                        CreatePipelineImpl(const GPUPipelineDesc& desc) override;
    GPUSampler*                         CreateSamplerImpl(const GPUSamplerDesc& desc) override;

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
    VulkanDescriptorPool&               GetDescriptorPool() const       { return *mDescriptorPool; }
    VulkanGeometryPool&                 GetGeometryPool() const         { return *mGeometryPool; }

    bool                                HasCap(const Caps cap) const
                                            { return (mCaps & cap) == cap; }

    /**
     * Get the current frame index (between 0 and kVulkanInFlightFrameCount),
     * for indexing data tracked for in-flight frames.
     */
    uint8_t                             GetCurrentFrame() const         { return mCurrentFrame; }

    /**
     * Add a function to be called when the current frame has completed on the
     * GPU. This can be used for deferred deletion.
     */
    void                                AddFrameCompleteCallback(FrameCompleteCallback callback);

    /** Apply a debug name to an object if we have VK_EXT_debug_marker. */
    template <typename T>
    void                                UpdateName(const T                          handle,
                                                   const VkDebugReportObjectTypeEXT type,
                                                   const std::string&               name);

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

    VkPipelineLayout                    GetPipelineLayout(const VulkanPipelineLayoutKey& key);

    /**
     * Get a Vulkan render pass and framebuffer object matching the given
     * render pass description from a cache. If no matching objects are found,
     * new ones will be created. Must be called from the main thread.
     */
    void                                GetRenderPass(const GPURenderPass& renderPass,
                                                      VkRenderPass&        outVulkanRenderPass,
                                                      VkFramebuffer&       outFramebuffer);

    /**
     * Get a Vulkan render pass matching the given render target state, which
     * should be compatible with any real render pass matching the state.
     */
    VkRenderPass                        GetRenderPass(const GPURenderTargetStateDesc& state);

    /**
     * Callback from VulkanResourceView and VulkanSwapchain to invalidate any
     * framebuffers referring to a view.
     */
    void                                InvalidateFramebuffers(const VkImageView view);

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

    using PipelineLayoutCache         = HashMap<VulkanPipelineLayoutKey, VkPipelineLayout>;
    using FramebufferCache            = HashMap<VulkanFramebufferKey, VkFramebuffer>;
    using RenderPassCache             = HashMap<VulkanRenderPassKey, VkRenderPass>;

private:
    void                                CreateDevice();

    VkRenderPass                        GetRenderPass(const VulkanRenderPassKey& key);

private:
    VulkanInstance*                     mInstance;

    VkPhysicalDevice                    mPhysicalDevice;
    VkDevice                            mHandle;
    uint32_t                            mGraphicsQueueFamily;
    VkPhysicalDeviceProperties          mProperties;
    VkPhysicalDeviceFeatures            mFeatures;
    uint32_t                            mCaps;

    VkPipelineCache                     mDriverPipelineCache;

    VulkanMemoryManager*                mMemoryManager;
    VulkanDescriptorPool*               mDescriptorPool;
    VulkanGeometryPool*                 mGeometryPool;

    /**
     * Per-frame data. Indexed by mCurrentFrame, which is a value between 0
     * and kVulkanInFlightFrameCount.
     */
    uint8_t                             mCurrentFrame;
    Frame                               mFrames[kVulkanInFlightFrameCount];
    std::mutex                          mCompleteCallbacksLock;

    VulkanContext*                      mContexts[kVulkanMaxContexts];

    std::vector<VkSemaphore>            mSemaphorePool;
    std::vector<VkFence>                mFencePool;

    std::mutex                          mCacheLock;
    PipelineLayoutCache                 mPipelineLayoutCache;
    RenderPassCache                     mRenderPassCache;
    FramebufferCache                    mFramebufferCache;

    const VulkanArgumentSetLayout*      mDummyArgumentSetLayout;

};

template <typename T>
inline void VulkanDevice::UpdateName(const T                          handle,
                                     const VkDebugReportObjectTypeEXT type,
                                     const std::string&               name)
{
    if (HasCap(kCap_DebugMarker))
    {
        VkDebugMarkerObjectNameInfoEXT nameInfo = {};
        nameInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType  = type;
        nameInfo.object      = reinterpret_cast<uint64_t>(handle);
        nameInfo.pObjectName = name.c_str();

        vkDebugMarkerSetObjectNameEXT(GetHandle(), &nameInfo);
    }
}
