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

#include "VulkanDevice.h"

#include "VulkanArgumentSet.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanDescriptorPool.h"
#include "VulkanFormat.h"
#include "VulkanMemoryManager.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanResourceView.h"
#include "VulkanSampler.h"
#include "VulkanShader.h"
#include "VulkanStagingPool.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"
#include "VulkanTransientPool.h"
#include "VulkanUtils.h"

#include "Core/Utility.h"

#include <unordered_set>
#include <limits>
#include <vector>

static const char* kRequiredDeviceExtensions[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,

    /* We are targeting 1.1 and these extensions should be in core, however the
     * VulkanMemoryAllocator library uses them under their KHR aliases so
     * enable them explicitly. */
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
};

VulkanDevice::VulkanDevice() :
    mHandle         (VK_NULL_HANDLE),
    mCaps           (0),
    mCurrentFrame   (0),
    mContexts       ()
{
    if (!VulkanInstance::HasInstance())
    {
        /* Create the global Vulkan instance. */
        new VulkanInstance();
    }

    mInstance = &VulkanInstance::Get();

    CreateDevice();

    uint8_t nextContextID = 0;
    auto CreateContext =
        [&] (const uint32_t queueFamily)
        {
            auto context = new VulkanContext(*this, nextContextID, queueFamily);
            mContexts[nextContextID++] = context;
            return context;
        };

    mGraphicsContext = CreateContext(GetGraphicsQueueFamily());
    //mComputeContext  = CreateContext(GetComputeQueueFamily());
    //mTransferContext = CreateContext(GetTransferQueueFamily());

    /* Create a pipeline cache. TODO: Serialise this to disk on drivers that
     * don't have their own on-disk cache. */
    VkPipelineCacheCreateInfo cacheCreateInfo = {};
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    VulkanCheck(vkCreatePipelineCache(GetHandle(),
                                      &cacheCreateInfo,
                                      nullptr,
                                      &mDriverPipelineCache));

    mMemoryManager  = new VulkanMemoryManager(*this);
    mDescriptorPool = new VulkanDescriptorPool(*this);
    mStagingPool    = new VulkanStagingPool(*this);
    mConstantPool   = new VulkanConstantPool(*this);
    mGeometryPool   = new VulkanGeometryPool(*this);

    /* See GetPipelineLayout() for details of what this is for. */
    GPUArgumentSetLayoutDesc layoutDesc;
    mDummyArgumentSetLayout = static_cast<const VulkanArgumentSetLayout*>(GetArgumentSetLayout(std::move(layoutDesc)));
}

VulkanDevice::~VulkanDevice()
{
    vkDeviceWaitIdle(mHandle);

    DestroyResources();

    for (const auto& it : mFramebufferCache)
    {
        vkDestroyFramebuffer(mHandle, it.second, nullptr);
    }

    for (const auto& it : mRenderPassCache)
    {
        vkDestroyRenderPass(mHandle, it.second, nullptr);
    }

    for (size_t i = 0; i < ArraySize(mFrames); i++)
    {
        auto& frame = mFrames[i];
        std::move(frame.semaphores.begin(), frame.semaphores.end(), std::back_inserter(mSemaphorePool));
        std::move(frame.fences.begin(), frame.fences.end(), std::back_inserter(mFencePool));

        for (const auto& callback : frame.completeCallbacks)
        {
            callback(*this);
        }

        frame.completeCallbacks.clear();
    }

    for (VkSemaphore semaphore : mSemaphorePool)
    {
        vkDestroySemaphore(mHandle, semaphore, nullptr);
    }

    for (VkFence fence : mFencePool)
    {
        vkDestroyFence(mHandle, fence, nullptr);
    }

    vkDestroyPipelineCache(GetHandle(), mDriverPipelineCache, nullptr);

    for (auto context : mContexts)
    {
        delete context;
    }

    delete mGeometryPool;
    delete static_cast<VulkanConstantPool*>(mConstantPool);
    delete static_cast<VulkanStagingPool*>(mStagingPool);
    delete mDescriptorPool;
    delete mMemoryManager;

    vkDestroyDevice(mHandle, nullptr);
}

void VulkanDevice::CreateDevice()
{
    uint32_t count;
    VulkanCheck(vkEnumeratePhysicalDevices(GetInstance().GetHandle(), &count, nullptr));

    if (count == 0)
    {
        Fatal("No Vulkan physical devices available");
    }

    std::vector<VkPhysicalDevice> devices(count);
    VulkanCheck(vkEnumeratePhysicalDevices(GetInstance().GetHandle(), &count, devices.data()));

    size_t deviceIndex = count;

    /* Pick a device. Use the first, but if it is not a discrete GPU then check
     * if there is one, and prefer it if so. */
    for (size_t i = 0; i < devices.size(); i++)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        const bool isDiscrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

        if (deviceIndex == count || isDiscrete)
        {
            deviceIndex = i;
            mProperties = properties;

            if (isDiscrete)
            {
                break;
            }
        }
    }

    LogInfo("Using device %zu (%s)", deviceIndex, mProperties.deviceName);
    mPhysicalDevice = devices[deviceIndex];

    LogInfo("API version: %u.%u.%u",
            VK_VERSION_MAJOR(mProperties.apiVersion),
            VK_VERSION_MINOR(mProperties.apiVersion),
            VK_VERSION_PATCH(mProperties.apiVersion));
    LogInfo("IDs:         0x%x / 0x%x",
            mProperties.vendorID,
            mProperties.deviceID);

    switch (mProperties.vendorID)
    {
        case 0x1002:    mVendor = kGPUVendor_AMD; break;
        case 0x8086:    mVendor = kGPUVendor_Intel; break;
        case 0x10de:    mVendor = kGPUVendor_NVIDIA; break;
    }

    if (mProperties.apiVersion < VK_API_VERSION_1_1)
    {
        Fatal("Vulkan 1.1 is not supported");
    }

    /* Enable all supported features, aside from robustBufferAccess - we should
     * behave properly without it, and it can have a performance impact. */
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
    mFeatures.robustBufferAccess = false;

    /* Find suitable queue families. We need to support both graphics
     * operations and presentation. */
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, nullptr);

    mGraphicsQueueFamily = std::numeric_limits<uint32_t>::max();

    if (count > 0)
    {
        std::vector<VkQueueFamilyProperties> familyProps(count);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, familyProps.data());

        for (uint32_t family = 0; family < familyProps.size(); family++)
        {
            const bool graphicsSupported = familyProps[family].queueCount > 0 &&
                                           familyProps[family].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                                           familyProps[family].queueFlags & VK_QUEUE_COMPUTE_BIT;

            const bool presentSupported = VulkanSwapchain::CheckPresentationSupport(mPhysicalDevice, family);

            if (graphicsSupported && presentSupported)
            {
                mGraphicsQueueFamily = family;
                break;
            }
        }
    }

    if (mGraphicsQueueFamily == std::numeric_limits<uint32_t>::max())
    {
        Fatal("No suitable graphics queue families");
    }

    LogInfo("Using graphics queue family %u", mGraphicsQueueFamily);

    std::unordered_set<std::string> availableExtensions;

    auto EnumerateExtensions =
        [&] (const char* const layerName)
        {
            VulkanCheck(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, layerName, &count, nullptr));
            std::vector<VkExtensionProperties> extensionProps(count);
            VulkanCheck(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, layerName, &count, extensionProps.data()));

            for (const VkExtensionProperties& extension : extensionProps)
            {
                LogInfo("  %s (revision %u)",
                        extension.extensionName,
                        extension.specVersion);

                availableExtensions.insert(extension.extensionName);
            }
        };

    LogInfo("Device extensions:");
    EnumerateExtensions(nullptr);

    for (const char* const layer : GetInstance().GetEnabledLayers())
    {
        LogInfo("Device extensions (%s):", layer);
        EnumerateExtensions(layer);
    }

    std::vector<const char*> enabledExtensions;

    auto EnableExtension =
        [&] (const char* const name, const uint32_t cap, const bool required = false) -> bool
        {
            const bool available = availableExtensions.find(name) != availableExtensions.end();

            if (available)
            {
                enabledExtensions.emplace_back(name);
                mCaps |= cap;
            }
            else if (required)
            {
                Fatal("Required Vulkan device extension '%s' not available", name);
            }

            return available;
        };

    for (const char* const extension : kRequiredDeviceExtensions)
    {
        EnableExtension(extension, 0, true);
    }

    #if GEMINI_GPU_MARKERS
        EnableExtension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, kCap_DebugMarker);
    #endif

    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = mGraphicsQueueFamily;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
    deviceCreateInfo.enabledLayerCount       = GetInstance().GetEnabledLayers().size();
    deviceCreateInfo.ppEnabledLayerNames     = GetInstance().GetEnabledLayers().data();
    deviceCreateInfo.enabledExtensionCount   = enabledExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    deviceCreateInfo.pEnabledFeatures        = &mFeatures;

    VulkanCheck(vkCreateDevice(mPhysicalDevice,
                               &deviceCreateInfo,
                               nullptr,
                               &mHandle));

    #define LOAD_OPTIONAL_VULKAN_DEVICE_FUNC(name) \
        name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(mHandle, #name));

    #define LOAD_VULKAN_DEVICE_FUNC(name) \
        LOAD_OPTIONAL_VULKAN_DEVICE_FUNC(name); \
        if (!name) \
        { \
            Fatal("Failed to load Vulkan function '%s'", #name); \
        }

    ENUMERATE_VULKAN_DEVICE_FUNCS(LOAD_VULKAN_DEVICE_FUNC);
    ENUMERATE_VULKAN_DEVICE_EXTENSION_FUNCS(LOAD_OPTIONAL_VULKAN_DEVICE_FUNC);

    #undef LOAD_VULKAN_DEVICE_FUNC
    #undef LOAD_OPTIONAL_VULKAN_DEVICE_FUNC
}

GPUArgumentSet* VulkanDevice::CreateArgumentSet(const GPUArgumentSetLayoutRef layout,
                                                const GPUArgument* const      arguments)
{
    return new VulkanArgumentSet(*this, layout, arguments);
}

GPUArgumentSetLayout* VulkanDevice::CreateArgumentSetLayoutImpl(GPUArgumentSetLayoutDesc&& desc)
{
    return new VulkanArgumentSetLayout(*this, std::move(desc));
}

GPUBuffer* VulkanDevice::CreateBuffer(const GPUBufferDesc& desc)
{
    return new VulkanBuffer(*this, desc);
}

GPUComputePipeline* VulkanDevice::CreateComputePipeline(const GPUComputePipelineDesc& desc)
{
    return new VulkanComputePipeline(*this, desc);
}

GPUPipeline* VulkanDevice::CreatePipelineImpl(const GPUPipelineDesc& desc)
{
    return new VulkanPipeline(*this, desc);
}

GPUResourceView* VulkanDevice::CreateResourceView(GPUResource* const         resource,
                                                  const GPUResourceViewDesc& desc)
{
    return new VulkanResourceView(*resource, desc);
}

GPUSampler* VulkanDevice::CreateSamplerImpl(const GPUSamplerDesc& desc)
{
    return new VulkanSampler(*this, desc);
}

GPUShaderPtr VulkanDevice::CreateShader(const GPUShaderStage stage,
                                        GPUShaderCode        code,
                                        const std::string&   function)
{
    return new VulkanShader(*this, stage, std::move(code), function);
}

void VulkanDevice::CreateSwapchain(Window& window)
{
    new VulkanSwapchain(*this, window);
}

GPUTexture* VulkanDevice::CreateTexture(const GPUTextureDesc& desc)
{
    return new VulkanTexture(*this, desc);
}

void VulkanDevice::EndFrameImpl()
{
    /* Submit outstanding work on all contexts. */
    for (auto context : mContexts)
    {
        if (context)
        {
            context->EndFrame();
        }
    }

    /* Advance to the next frame. */
    mCurrentFrame = (mCurrentFrame + 1) % kVulkanInFlightFrameCount;
    auto& frame = mFrames[mCurrentFrame];

    /* Wait for all submissions in frame to complete. TODO: Would it be worth
     * optimising this to only wait for the last fence in the frame (needs to
     * be careful for multi-queue, may need multiple there)? Not sure whether
     * there's much greater overhead for just passing all fences here. */
    if (!frame.fences.empty())
    {
        VulkanCheck(vkWaitForFences(mHandle,
                                    frame.fences.size(),
                                    frame.fences.data(),
                                    VK_TRUE,
                                    std::numeric_limits<uint64_t>::max()));

        VulkanCheck(vkResetFences(mHandle,
                                  frame.fences.size(),
                                  frame.fences.data()));
    }

    /* Move all semaphores and fences back to the pool to be reused. */
    std::move(frame.semaphores.begin(), frame.semaphores.end(), std::back_inserter(mSemaphorePool));
    frame.semaphores.clear();
    std::move(frame.fences.begin(), frame.fences.end(), std::back_inserter(mFencePool));
    frame.fences.clear();

    {
        std::unique_lock lock(mCompleteCallbacksLock);

        for (const auto& callback : frame.completeCallbacks)
        {
            callback(*this);
        }

        frame.completeCallbacks.clear();
    }

    /* Reset command pools etc. on each context. */
    for (auto context : mContexts)
    {
        if (context)
        {
            context->BeginFrame();
        }
    }

    static_cast<VulkanConstantPool&>(GetConstantPool()).BeginFrame();
    mGeometryPool->BeginFrame();
}

void VulkanDevice::AddFrameCompleteCallback(FrameCompleteCallback callback)
{
    std::unique_lock lock(mCompleteCallbacksLock);
    mFrames[mCurrentFrame].completeCallbacks.emplace_back(std::move(callback));
}

VkSemaphore VulkanDevice::AllocateSemaphore()
{
    VkSemaphore semaphore;

    /* No locking required - this should only be used on the main thread. */
    if (!mSemaphorePool.empty())
    {
        semaphore = mSemaphorePool.back();
        mSemaphorePool.pop_back();
    }
    else
    {
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VulkanCheck(vkCreateSemaphore(mHandle, &createInfo, nullptr, &semaphore));
    }

    mFrames[mCurrentFrame].semaphores.emplace_back(semaphore);
    return semaphore;
}

VkFence VulkanDevice::AllocateFence()
{
    VkFence fence;

    /* No locking required - this should only be used on the main thread. */
    if (!mFencePool.empty())
    {
        fence = mFencePool.back();
        mFencePool.pop_back();
    }
    else
    {
        VkFenceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VulkanCheck(vkCreateFence(mHandle, &createInfo, nullptr, &fence));
    }

    mFrames[mCurrentFrame].fences.emplace_back(fence);
    return fence;
}

VkPipelineLayout VulkanDevice::GetPipelineLayout(const VulkanPipelineLayoutKey& key)
{
    std::unique_lock lock(mCacheLock);

    VkPipelineLayout& cachedLayout = mPipelineLayoutCache[key];
    if (cachedLayout == VK_NULL_HANDLE)
    {
        VkDescriptorSetLayout setLayouts[kMaxArgumentSets];

        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType       = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.pSetLayouts = setLayouts;

        for (size_t i = 0; i < kMaxArgumentSets; i++)
        {
            const VulkanArgumentSetLayout* setLayout;
            if (key.argumentSetLayouts[i])
            {
                createInfo.setLayoutCount = i + 1;

                setLayout = static_cast<const VulkanArgumentSetLayout*>(key.argumentSetLayouts[i]);
            }
            else
            {
                /* If we have e.g. set 0 and set 2 populated, set 1 must still
                 * be supplied a valid VkDescriptorSetLayout handle, so we
                 * create a dummy empty set and pass it in to handle this. */
                setLayout = mDummyArgumentSetLayout;
            }

            setLayouts[i] = setLayout->GetHandle();
        }

        VulkanCheck(vkCreatePipelineLayout(GetHandle(),
                                           &createInfo,
                                           nullptr,
                                           &cachedLayout));
    }

    return cachedLayout;
}

/**
 * Standard render pass dependencies. We always use explicit external barriers
 * rather than handling synchronisation and layout transitions with the render
 * pass. However, when no external dependencies are specified in a pass, there
 * are some implicitly defined default ones. These don't really do much of
 * value, but do cause some extra synchronisation on some drivers. Therefore,
 * override them with truly useless barriers to avoid this extra sync.
 */
static const VkSubpassDependency kDefaultRenderPassDependencies[] =
{
    {
        VK_SUBPASS_EXTERNAL,
        0,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        0
    },
    {
        0,
        VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        0
    },
};

VkRenderPass VulkanDevice::GetRenderPass(const VulkanRenderPassKey& key)
{
    std::unique_lock lock(mCacheLock);

    VkRenderPass& cachedRenderPass = mRenderPassCache[key];

    if (cachedRenderPass == VK_NULL_HANDLE)
    {
        /* VkRenderPassCreateInfo requires a tightly packed attachment array,
         * which we map on to the correct indices in the subpass. */
        VkRenderPassCreateInfo createInfo = {};
        VkSubpassDescription subpass = {};
        VkAttachmentDescription attachments[kMaxRenderPassColourAttachments + 1] = {{}};
        VkAttachmentReference colourReferences[kMaxRenderPassColourAttachments];
        VkAttachmentReference depthStencilReference;

        auto AddAttachment = [&] (const VulkanRenderPassKey::Attachment& srcAttachment,
                                  VkAttachmentReference&                 outReference) -> bool
        {
            const bool result = srcAttachment.format != kPixelFormat_Unknown;

            if (result)
            {
                createInfo.pAttachments = attachments;

                VkImageLayout layout;

                switch (srcAttachment.state)
                {
                    case kGPUResourceState_RenderTarget:
                        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        break;

                    case kGPUResourceState_DepthStencilWrite:
                        layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        break;

                    case kGPUResourceState_DepthReadStencilWrite:
                        layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
                        break;

                    case kGPUResourceState_DepthWriteStencilRead:
                        layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
                        break;

                    case kGPUResourceState_DepthStencilRead:
                        layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                        break;

                    default:
                        UnreachableMsg("Invalid GPUResourceState for attachment");

                }

                outReference.attachment = createInfo.attachmentCount;
                outReference.layout     = layout;

                auto& attachment = attachments[createInfo.attachmentCount++];

                attachment.format         = VulkanFormat::GetVulkanFormat(srcAttachment.format);
                attachment.samples        = VK_SAMPLE_COUNT_1_BIT; // TODO
                attachment.loadOp         = VulkanUtils::ConvertLoadOp(srcAttachment.loadOp);
                attachment.stencilLoadOp  = VulkanUtils::ConvertLoadOp(srcAttachment.stencilLoadOp);
                attachment.storeOp        = VulkanUtils::ConvertStoreOp(srcAttachment.storeOp);
                attachment.stencilStoreOp = VulkanUtils::ConvertStoreOp(srcAttachment.stencilStoreOp);
                attachment.initialLayout  = layout;
                attachment.finalLayout    = layout;
            }
            else
            {
                outReference.attachment = VK_ATTACHMENT_UNUSED;
            }

            return result;
        };

        for (size_t i = 0; i < kMaxRenderPassColourAttachments; i++)
        {
            if (AddAttachment(key.colour[i], colourReferences[i]))
            {
                subpass.colorAttachmentCount = i + 1;
                subpass.pColorAttachments    = colourReferences;
            }
        }

        if (AddAttachment(key.depthStencil, depthStencilReference))
        {
            subpass.pDepthStencilAttachment = &depthStencilReference;
        }

        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        createInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.subpassCount    = 1;
        createInfo.pSubpasses      = &subpass;
        createInfo.dependencyCount = ArraySize(kDefaultRenderPassDependencies);
        createInfo.pDependencies   = kDefaultRenderPassDependencies;

        VulkanCheck(vkCreateRenderPass(GetHandle(),
                                       &createInfo,
                                       nullptr,
                                       &cachedRenderPass));
    }

    return cachedRenderPass;
}

void VulkanDevice::GetRenderPass(const GPURenderPass& pass,
                                 VkRenderPass&        outVulkanRenderPass,
                                 VkFramebuffer&       outFramebuffer)
{
    VulkanRenderPassKey passKey(pass);

    outVulkanRenderPass = GetRenderPass(passKey);

    VulkanFramebufferKey framebufferKey(pass);

    std::unique_lock lock(mCacheLock);

    VkFramebuffer& cachedFramebuffer = mFramebufferCache[framebufferKey];

    if (cachedFramebuffer == VK_NULL_HANDLE)
    {
        /* Same as above. The indices into the view array here must match up
         * with the tightly packed attachment array in the render pass. */
        VkFramebufferCreateInfo createInfo = {};
        VkImageView imageViews[kMaxRenderPassColourAttachments + 1];

        for (size_t i = 0; i < kMaxRenderPassColourAttachments; i++)
        {
            const auto view = static_cast<const VulkanResourceView*>(pass.colour[i].view);

            if (view)
            {
                imageViews[createInfo.attachmentCount++] = view->GetImageView();
            }
        }

        const auto view = static_cast<const VulkanResourceView*>(pass.depthStencil.view);

        if (view)
        {
            imageViews[createInfo.attachmentCount++] = view->GetImageView();
        }

        pass.GetDimensions(createInfo.width,
                           createInfo.height,
                           createInfo.layers);

        createInfo.sType        = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass   = outVulkanRenderPass;
        createInfo.pAttachments = imageViews;

        VulkanCheck(vkCreateFramebuffer(GetHandle(),
                                        &createInfo,
                                        nullptr,
                                        &cachedFramebuffer));
    }

    outFramebuffer = cachedFramebuffer;
}

VkRenderPass VulkanDevice::GetRenderPass(const GPURenderTargetStateDesc& state)
{
    VulkanRenderPassKey passKey(state);
    return GetRenderPass(passKey);
}

void VulkanDevice::InvalidateFramebuffers(const VkImageView view)
{
    std::unique_lock lock(mCacheLock);

    for (auto it = mFramebufferCache.begin(); it != mFramebufferCache.end(); )
    {
        bool isMatch = it->first.depthStencil == view;

        if (!isMatch)
        {
            for (VkImageView colourView : it->first.colour)
            {
                isMatch = colourView == view;
                if (isMatch)
                {
                    break;
                }
            }
        }

        if (isMatch)
        {
            AddFrameCompleteCallback(
                [framebuffer = it->second] (VulkanDevice& device)
                {
                    vkDestroyFramebuffer(device.GetHandle(), framebuffer, nullptr);
                });

            it = mFramebufferCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
