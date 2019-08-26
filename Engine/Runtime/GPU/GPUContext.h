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
#include "GPU/GPUCommandList.h"
#include "GPU/GPURenderPass.h"

class GPUStagingBuffer;
class GPUStagingTexture;
class GPUSwapchain;
class GPUTexture;

/**
 * Base class for a context used for submitting work to a GPU queue. A device
 * up to 3 contexts:
 *
 *  - A graphics context (GPUGraphicsContext). Always present, represents the
 *    main graphics queue.
 *  - A compute context (GPUComputeContext). Optional, represents an
 *    asynchronous compute queue if available.
 *  - A transfer context (GPUTransferContext). Optional, represents a dedicated
 *    transfer queue if available.
 *
 * Contexts should only be used from the main thread. Multithreaded command
 * recording is available within render/compute passes: these give you a
 * GPUCommandList to record to, which can be used from other threads, and can
 * have child command lists created to allow multithreading within a pass.
 */
class GPUContext : public GPUDeviceChild
{
protected:
                                    GPUContext(GPUDevice& inDevice);

public:
                                    ~GPUContext() {}

public:
    /**
     * Add a GPU-side dependency between this context and another. All work
     * submitted to this context after a call to this function will not begin
     * execution on the GPU until all work submitted to the other context prior
     * to this call has completed.
     */
    virtual void                    Wait(GPUContext& inOtherContext) = 0;

protected:
    /**
     * Called by every public context method to verify that the context is
     * being used from the main thread. Does nothing on non-debug builds.
     */
    void                            ValidateContext() const;

};

class GPUTransferContext : public GPUContext
{
protected:
                                    GPUTransferContext(GPUDevice& inDevice);

public:
                                    ~GPUTransferContext() {}

public:
    /**
     * Transition (sub)resources between states. See GPUResourceBarrier and
     * GPUResourceState for more details. Barriers should be batched together
     * into a single call to this wherever possible.
     */
    virtual void                    ResourceBarrier(const GPUResourceBarrier* const inBarriers,
                                                    const size_t                    inCount) = 0;
    void                            ResourceBarrier(GPUResource* const     inResource,
                                                    const GPUResourceState inCurrentState,
                                                    const GPUResourceState inNewState,
                                                    const bool             inDiscard = false);
    void                            ResourceBarrier(GPUResourceView* const inView,
                                                    const GPUResourceState inCurrentState,
                                                    const GPUResourceState inNewState,
                                                    const bool             inDiscard = false);

    /**
     * Clear a texture. This is a standalone clear, which requires the cleared
     * range to be in the kGPUResourceState_TransferWrite state. It should be
     * preferred to clear render target and depth/stencil textures as part of a
     * render pass to them, as this is likely more efficient than doing an
     * explicit clear outside the pass.
     */
    virtual void                    ClearTexture(GPUTexture* const          inTexture,
                                                 const GPUTextureClearData& inData,
                                                 const GPUSubresourceRange& inRange = GPUSubresourceRange()) = 0;

    /**
     * Upload data to a buffer from a staging buffer. Requires the destination
     * buffer to be in the kGPUResourceState_TransferWrite state.
     *
     * TODO: Have an upload command list that allows resource uploads to be
     * done outside the main thread (for async resource loading).
     */
    virtual void                    UploadBuffer(GPUBuffer* const        inDestBuffer,
                                                 const GPUStagingBuffer& inSourceBuffer,
                                                 const uint32_t          inSize,
                                                 const uint32_t          inDestOffset   = 0,
                                                 const uint32_t          inSourceOffset = 0) = 0;

    /**
     * Upload data to a texture from a staging texture. Requires the destination
     * texture to be in the kGPUResourceState_TransferWrite state. Format must
     * match between source and destination.
     *
     * The first version requires a match in dimension/subresource count between
     * source and destination, and will upload the whole texture.
     *
     * The second version uploads a subregion of a single subresource, and only
     * requires that the specified region is valid within both the source and
     * destination subresource.
     *
     * TODO: Have an upload command list that allows resource uploads to be
     * done outside the main thread (for async resource loading).
     */
    virtual void                    UploadTexture(GPUTexture* const        inDestTexture,
                                                  const GPUStagingTexture& inSourceTexture) = 0;
    virtual void                    UploadTexture(GPUTexture* const        inDestTexture,
                                                  const GPUStagingTexture& inSourceTexture,
                                                  const glm::ivec3&        inSize,
                                                  const GPUSubresource     inDestSubresource,
                                                  const glm::ivec3&        inDestOffset,
                                                  const GPUSubresource     inSourceSubresource,
                                                  const glm::ivec3&        inSourceOffset) = 0;

};

class GPUComputeContext : public GPUTransferContext
{
protected:
                                    GPUComputeContext(GPUDevice& inDevice);

public:
                                    ~GPUComputeContext() {}

public:
    /**
     * Interface for presenting to a swapchain. See GPUSwapchain for more
     * details. BeginPresent() must be called before using a swapchain's
     * texture. EndPresent() will present whatever has been rendered to the
     * texture to the swapchain's window. The swapchain must not be used on
     * any other thread, or any other context, between these calls.
     *
     * After BeginPresent() returns, the whole swapchain texture will be in
     * the kGPUResourceState_Present state. It must be returned to this state
     * before EndPresent() is called.
     */
    virtual void                    BeginPresent(GPUSwapchain& inSwapchain) = 0;
    virtual void                    EndPresent(GPUSwapchain& inSwapchain) = 0;

    /**
     * Create a compute pass. This does not perform any work on the context,
     * rather it returns a GPUComputeCommandList to record the commands for the
     * pass on. Pass the command list to SubmitComputePass() once all commands
     * have been recorded.
     */
    GPUComputeCommandList*          CreateComputePass();

    /**
     * Submit a compute pass. Passes need not be submitted in the same order
     * they were created in, however they must be submitted within the same
     * frame as they were created.
     */
    void                            SubmitComputePass(GPUComputeCommandList* const inCmdList);

protected:
    virtual GPUComputeCommandList*  CreateComputePassImpl() = 0;
    virtual void                    SubmitComputePassImpl(GPUComputeCommandList* const inCmdList) = 0;

protected:
    #if GEMINI_BUILD_DEBUG

    /** Number of active passes (used to ensure command lists don't leak). */
    uint32_t                        mActivePassCount;

    #endif

    friend class GPUDevice;
};

class GPUGraphicsContext : public GPUComputeContext
{
protected:
                                    GPUGraphicsContext(GPUDevice& inDevice);

public:
                                    ~GPUGraphicsContext() {}

public:
    static GPUGraphicsContext&      Get()   { return GPUDevice::Get().GetGraphicsContext(); }

    /**
     * Create a render pass. This does not perform any work on the context,
     * rather it returns a GPUGraphicsCommandList to record the commands for
     * the pass on. Pass the command list to SubmitRenderPass() once all
     * commands have been recorded.
     */
    GPUGraphicsCommandList*         CreateRenderPass(const GPURenderPass& inRenderPass);

    /**
     * Submit a render pass. Passes need not be submitted in the same order
     * they were created in, however they must be submitted within the same
     * frame as they were created.
     */
    void                            SubmitRenderPass(GPUGraphicsCommandList* const inCmdList);

protected:
    virtual GPUGraphicsCommandList* CreateRenderPassImpl(const GPURenderPass& inRenderPass) = 0;
    virtual void                    SubmitRenderPassImpl(GPUGraphicsCommandList* const inCmdList) = 0;

};

inline GPUContext::GPUContext(GPUDevice& inDevice) :
    GPUDeviceChild      (inDevice)
{
}

inline void GPUContext::ValidateContext() const
{
    Assert(Thread::IsMain());
}

inline GPUTransferContext::GPUTransferContext(GPUDevice& inDevice) :
    GPUContext          (inDevice)
{
}

inline void GPUTransferContext::ResourceBarrier(GPUResource* const     inResource,
                                                const GPUResourceState inCurrentState,
                                                const GPUResourceState inNewState,
                                                const bool             inDiscard)
{
    GPUResourceBarrier barrier = {};
    barrier.resource     = inResource;
    barrier.currentState = inCurrentState;
    barrier.newState     = inNewState;
    barrier.discard      = inDiscard;

    ResourceBarrier(&barrier, 1);
}

inline void GPUTransferContext::ResourceBarrier(GPUResourceView* const inView,
                                                const GPUResourceState inCurrentState,
                                                const GPUResourceState inNewState,
                                                const bool             inDiscard)
{
    GPUResourceBarrier barrier = {};
    barrier.resource     = &inView->GetResource();
    barrier.range        = inView->GetSubresourceRange();
    barrier.currentState = inCurrentState;
    barrier.newState     = inNewState;
    barrier.discard      = inDiscard;

    ResourceBarrier(&barrier, 1);
}

inline GPUComputeContext::GPUComputeContext(GPUDevice& inDevice) :
    GPUTransferContext  (inDevice)
{
    #if GEMINI_BUILD_DEBUG
        mActivePassCount = 0;
    #endif
}

inline GPUComputeCommandList* GPUComputeContext::CreateComputePass()
{
    ValidateContext();

    #if GEMINI_BUILD_DEBUG
        mActivePassCount++;
    #endif

    return CreateComputePassImpl();
}

inline void GPUComputeContext::SubmitComputePass(GPUComputeCommandList* const inCmdList)
{
    ValidateContext();

    Assert(!inCmdList->GetParent());
    Assert(inCmdList->GetState() == GPUCommandList::kState_Ended);

    SubmitComputePassImpl(inCmdList);

    #if GEMINI_BUILD_DEBUG
        mActivePassCount--;
    #endif
}

inline GPUGraphicsContext::GPUGraphicsContext(GPUDevice& inDevice) :
    GPUComputeContext  (inDevice)
{
}

inline GPUGraphicsCommandList* GPUGraphicsContext::CreateRenderPass(const GPURenderPass& inRenderPass)
{
    ValidateContext();

    #if GEMINI_BUILD_DEBUG
        mActivePassCount++;
        inRenderPass.Validate();
    #endif

    return CreateRenderPassImpl(inRenderPass);
}

inline void GPUGraphicsContext::SubmitRenderPass(GPUGraphicsCommandList* const inCmdList)
{
    ValidateContext();

    Assert(!inCmdList->GetParent());
    Assert(inCmdList->GetState() == GPUCommandList::kState_Ended);

    SubmitRenderPassImpl(inCmdList);

    #if GEMINI_BUILD_DEBUG
        mActivePassCount--;
    #endif
}
