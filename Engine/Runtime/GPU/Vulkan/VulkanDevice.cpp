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

#include "VulkanDevice.h"

#include "VulkanArgumentSet.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanFormat.h"
#include "VulkanMemoryManager.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanResourceView.h"
#include "VulkanShader.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"
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
        [&] (const uint32_t inQueueFamily)
        {
            auto context = new VulkanContext(*this, nextContextID, inQueueFamily);
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

    mMemoryManager = new VulkanMemoryManager(*this);

    /* See GetPipelineLayout() for details of what this is for. */
    GPUArgumentSetLayoutDesc layoutDesc;
    mDummyArgumentSetLayout = static_cast<VulkanArgumentSetLayout*>(GetArgumentSetLayout(std::move(layoutDesc)));
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

    mPhysicalDevice = VK_NULL_HANDLE;

    std::unordered_set<std::string> availableExtensions;
    std::vector<VkExtensionProperties> extensionProps;

    for (size_t i = 0; i < devices.size(); i++)
    {
        VkPhysicalDevice device = devices[i];

        vkGetPhysicalDeviceProperties(device, &mProperties);
        vkGetPhysicalDeviceFeatures(device, &mFeatures);

        LogInfo("Checking device %zu (%s)", i, mProperties.deviceName);

        if (mProperties.apiVersion < VK_API_VERSION_1_1)
        {
            LogWarning("  Vulkan 1.1 is not supported");
            continue;
        }

        VulkanCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr));
        extensionProps.resize(count);
        VulkanCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensionProps.data()));

        availableExtensions.clear();
        for (const VkExtensionProperties& extension : extensionProps)
        {
            availableExtensions.insert(extension.extensionName);
        }

        bool found = true;

        for (const char* extension : kRequiredDeviceExtensions)
        {
            found = availableExtensions.find(extension) != availableExtensions.end();
            if (!found)
            {
                LogWarning("  Required device extension '%s' not available", extension);
                break;
            }
        }

        if (!found)
        {
            continue;
        }

        /* Find suitable queue families. We need to support both graphics
         * operations and presentation. */
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

        mGraphicsQueueFamily = std::numeric_limits<uint32_t>::max();

        if (count > 0)
        {
            std::vector<VkQueueFamilyProperties> familyProps(count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, familyProps.data());

            for (uint32_t family = 0; family < familyProps.size(); family++)
            {
                const bool graphicsSupported = familyProps[family].queueCount > 0 &&
                                               familyProps[family].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                                               familyProps[family].queueFlags & VK_QUEUE_COMPUTE_BIT;

                const bool presentSupported  = VulkanSwapchain::CheckPresentationSupport(device, i);

                if (graphicsSupported && presentSupported)
                {
                    mGraphicsQueueFamily = family;
                    break;
                }
            }
        }

        if (mGraphicsQueueFamily == std::numeric_limits<uint32_t>::max())
        {
            LogWarning("  No suitable queue families");
            continue;
        }

        /* This device is good. */
        mPhysicalDevice = device;
        break;
    }

    if (mPhysicalDevice == VK_NULL_HANDLE)
    {
        Fatal("No suitable Vulkan devices found");
    }

    LogInfo("  API version: %u.%u.%u",
            VK_VERSION_MAJOR(mProperties.apiVersion),
            VK_VERSION_MINOR(mProperties.apiVersion),
            VK_VERSION_PATCH(mProperties.apiVersion));
    LogInfo("  IDs:         0x%x / 0x%x",
            mProperties.vendorID,
            mProperties.deviceID);

    switch (mProperties.vendorID)
    {
        case 0x1002:    mVendor = kGPUVendor_AMD; break;
        case 0x8086:    mVendor = kGPUVendor_Intel; break;
        case 0x10de:    mVendor = kGPUVendor_NVIDIA; break;
    }

    LogInfo("  Extensions:");

    for (const VkExtensionProperties& extension : extensionProps)
    {
        LogInfo("    %s (revision %u)",
                extension.extensionName,
                extension.specVersion);
    }

    std::vector<const char*> enabledLayers;
    std::vector<const char*> enabledExtensions;

    if (GetInstance().HasCap(VulkanInstance::kCap_Validation))
    {
        /* Assume that if the instance has validation then the device does
         * too. */
        enabledLayers.emplace_back("VK_LAYER_LUNARG_standard_validation");
    }

    enabledExtensions.assign(&kRequiredDeviceExtensions[0],
                             &kRequiredDeviceExtensions[ArraySize(kRequiredDeviceExtensions)]);

    /* Enable all supported features, aside from robustBufferAccess - we
     * should behave properly without it, and it can have a performance
     * impact. */
    mFeatures.robustBufferAccess = false;

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
    deviceCreateInfo.enabledLayerCount       = enabledLayers.size();
    deviceCreateInfo.ppEnabledLayerNames     = enabledLayers.data();
    deviceCreateInfo.enabledExtensionCount   = enabledExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    deviceCreateInfo.pEnabledFeatures        = &mFeatures;;

    VulkanCheck(vkCreateDevice(mPhysicalDevice,
                               &deviceCreateInfo,
                               nullptr,
                               &mHandle));

    #define LOAD_VULKAN_DEVICE_FUNC(name) \
        name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(mHandle, #name)); \
        if (!name) \
        { \
            Fatal("Failed to load Vulkan function '%s'", #name); \
        }

    ENUMERATE_VULKAN_DEVICE_FUNCS(LOAD_VULKAN_DEVICE_FUNC);

    #undef LOAD_VULKAN_DEVICE_FUNC
}

GPUArgumentSetLayout* VulkanDevice::CreateArgumentSetLayoutImpl(GPUArgumentSetLayoutDesc&& inDesc)
{
    return new VulkanArgumentSetLayout(*this, std::move(inDesc));
}

GPUBufferPtr VulkanDevice::CreateBuffer(const GPUBufferDesc& inDesc)
{
    return new VulkanBuffer(*this, inDesc);
}

GPUPipeline* VulkanDevice::CreatePipelineImpl(const GPUPipelineDesc& inDesc)
{
    return new VulkanPipeline(*this, inDesc);
}

GPUResourceViewPtr VulkanDevice::CreateResourceView(GPUResource&               inResource,
                                                    const GPUResourceViewDesc& inDesc)
{
    return new VulkanResourceView(inResource, inDesc);
}

GPUShaderPtr VulkanDevice::CreateShader(const GPUShaderStage inStage,
                                        GPUShaderCode        inCode)
{
    return new VulkanShader(*this, inStage, std::move(inCode));
}

void VulkanDevice::CreateSwapchain(Window& inWindow)
{
    new VulkanSwapchain(*this, inWindow);
}

GPUTexturePtr VulkanDevice::CreateTexture(const GPUTextureDesc& inDesc)
{
    return new VulkanTexture(*this, inDesc);
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

    for (const auto& callback : frame.completeCallbacks)
    {
        callback(*this);
    }

    frame.completeCallbacks.clear();

    /* Reset command pools etc. on each context. */
    for (auto context : mContexts)
    {
        if (context)
        {
            context->BeginFrame();
        }
    }
}

void VulkanDevice::AddFrameCompleteCallback(FrameCompleteCallback inCallback)
{
    mFrames[mCurrentFrame].completeCallbacks.emplace_back(std::move(inCallback));
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

VkPipelineLayout VulkanDevice::GetPipelineLayout(const VulkanPipelineLayoutKey& inKey)
{
    std::unique_lock lock(mCacheLock);

    VkPipelineLayout& cachedLayout = mPipelineLayoutCache[inKey];
    if (cachedLayout == VK_NULL_HANDLE)
    {
        VkDescriptorSetLayout setLayouts[kMaxArgumentSets];

        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType       = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.pSetLayouts = setLayouts;

        for (size_t i = 0; i < kMaxArgumentSets; i++)
        {
            VulkanArgumentSetLayout* setLayout;
            if (inKey.argumentSetLayouts[i])
            {
                createInfo.setLayoutCount = i + 1;

                setLayout = static_cast<VulkanArgumentSetLayout*>(inKey.argumentSetLayouts[i]);
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

VkRenderPass VulkanDevice::GetRenderPass(const VulkanRenderPassKey& inKey)
{
    std::unique_lock lock(mCacheLock);

    VkRenderPass& cachedRenderPass = mRenderPassCache[inKey];
    if (cachedRenderPass == VK_NULL_HANDLE)
    {
        /* VkRenderPassCreateInfo requires a tightly packed attachment array,
         * which we map on to the correct indices in the subpass. */
        VkRenderPassCreateInfo createInfo = {};
        VkSubpassDescription subpass = {};
        VkAttachmentDescription attachments[kMaxRenderPassColourAttachments + 1] = {{}};
        VkAttachmentReference colourReferences[kMaxRenderPassColourAttachments];
        VkAttachmentReference depthStencilReference;

        auto AddAttachment =
            [&] (const VulkanRenderPassKey::Attachment& inSrcAttachment,
                 VkAttachmentReference&                 outReference) -> bool
            {
                const bool result = inSrcAttachment.format != PixelFormat::kUnknown;

                if (result)
                {
                    createInfo.pAttachments = attachments;

                    VkImageLayout layout;

                    switch (inSrcAttachment.state)
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

                    attachment.format         = VulkanFormat::GetVulkanFormat(inSrcAttachment.format);
                    attachment.samples        = VK_SAMPLE_COUNT_1_BIT; // TODO
                    attachment.loadOp         = VulkanUtils::ConvertLoadOp(inSrcAttachment.loadOp);
                    attachment.stencilLoadOp  = VulkanUtils::ConvertLoadOp(inSrcAttachment.stencilLoadOp);
                    attachment.storeOp        = VulkanUtils::ConvertStoreOp(inSrcAttachment.storeOp);
                    attachment.stencilStoreOp = VulkanUtils::ConvertStoreOp(inSrcAttachment.stencilStoreOp);
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
            if (AddAttachment(inKey.colour[i], colourReferences[i]))
            {
                subpass.colorAttachmentCount = i + 1;
                subpass.pColorAttachments    = colourReferences;
            }
        }

        if (AddAttachment(inKey.depthStencil, depthStencilReference))
        {
            subpass.pDepthStencilAttachment = &depthStencilReference;
        }

        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        createInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses   = &subpass;

        VulkanCheck(vkCreateRenderPass(GetHandle(),
                                       &createInfo,
                                       nullptr,
                                       &cachedRenderPass));
    }

    return cachedRenderPass;
}

void VulkanDevice::GetRenderPass(const GPURenderPass& inPass,
                                 VkRenderPass&        outVulkanRenderPass,
                                 VkFramebuffer&       outFramebuffer)
{
    VulkanRenderPassKey passKey(inPass);

    outVulkanRenderPass = GetRenderPass(passKey);

    VulkanFramebufferKey framebufferKey(inPass);

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
            const auto view = static_cast<const VulkanResourceView*>(inPass.colour[i].view);

            if (view)
            {
                imageViews[createInfo.attachmentCount++] = view->GetImageView();
            }
        }

        const auto view = static_cast<const VulkanResourceView*>(inPass.depthStencil.view);

        if (view)
        {
            imageViews[createInfo.attachmentCount++] = view->GetImageView();
        }

        inPass.GetDimensions(createInfo.width,
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

VkRenderPass VulkanDevice::GetRenderPass(const GPURenderTargetStateDesc& inState)
{
    VulkanRenderPassKey passKey(inState);
    return GetRenderPass(passKey);
}

void VulkanDevice::InvalidateFramebuffers(const VkImageView inView)
{
    std::unique_lock lock(mCacheLock);

    for (auto it = mFramebufferCache.begin(); it != mFramebufferCache.end(); )
    {
        bool isMatch = it->first.depthStencil == inView;

        if (!isMatch)
        {
            for (VkImageView colourView : it->first.colour)
            {
                isMatch = colourView == inView;
                if (isMatch)
                {
                    break;
                }
            }
        }

        if (isMatch)
        {
            AddFrameCompleteCallback(
                [framebuffer = it->second] (VulkanDevice& inDevice)
                {
                    vkDestroyFramebuffer(inDevice.GetHandle(), framebuffer, nullptr);
                });

            it = mFramebufferCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
