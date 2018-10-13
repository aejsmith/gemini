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

#include "GPU/GPUCommandList.h"

#include "GPU/GPUContext.h"

GPUCommandList::GPUCommandList(GPUContext&                 inContext,
                               const GPUCommandList* const inParent) :
    GPUDeviceChild  (inContext.GetDevice()),
    mContext        (inContext),
    mParent         (inParent),
    mState          (kState_Created)
{
    #if ORION_BUILD_DEBUG
        mActiveChildCount.store(0, std::memory_order_relaxed);
    #endif
}

GPUGraphicsCommandList::GPUGraphicsCommandList(GPUGraphicsContext&                 inContext,
                                               const GPUGraphicsCommandList* const inParent,
                                               const GPURenderPass&                inRenderPass) :
    GPUCommandList      (inContext, inParent),
    mRenderPass         (inRenderPass),
    mRenderTargetState  ((inParent)
                             ? inParent->GetRenderTargetState()
                             : mRenderPass.GetRenderTargetState()),
    mDirtyState         (0),
    mPipeline           (nullptr)
{
    /* Add a reference to each view in the pass. Only the top level command
     * list in a pass needs to do this - it will be alive as long as any
     * children are. */
    if (!mParent)
    {
        for (size_t i = 0; i < ArraySize(mRenderPass.colour); i++)
        {
            if (mRenderPass.colour[i].view)
            {
                mRenderPass.colour[i].view->Retain();
            }
        }

        if (mRenderPass.depthStencil.view)
        {
            mRenderPass.depthStencil.view->Retain();
        }
    }

    /* Initialise the viewport and scissor to the size of the render target. */
    uint32_t width, height, layers;
    inRenderPass.GetDimensions(width, height, layers);

    mViewport.rect.x      = mScissor.x      = 0;
    mViewport.rect.y      = mScissor.y      = 0;
    mViewport.rect.width  = mScissor.width  = width;
    mViewport.rect.height = mScissor.height = height;
    mViewport.minDepth    = 0.0f;
    mViewport.maxDepth    = 1.0f;

    mDirtyState |= kDirtyState_Viewport | kDirtyState_Scissor;
}

GPUGraphicsCommandList::~GPUGraphicsCommandList()
{
    if (!mParent)
    {
        for (size_t i = 0; i < ArraySize(mRenderPass.colour); i++)
        {
            if (mRenderPass.colour[i].view)
            {
                mRenderPass.colour[i].view->Release();
            }
        }

        if (mRenderPass.depthStencil.view)
        {
            mRenderPass.depthStencil.view->Release();
        }
    }
}

void GPUGraphicsCommandList::SetPipeline(GPUPipeline* const inPipeline)
{
    if (inPipeline != mPipeline)
    {
        mPipeline = inPipeline;

        mDirtyState |= kDirtyState_Pipeline;
    }
}

void GPUGraphicsCommandList::SetPipeline(const GPUPipelineDesc& inDesc)
{
    SetPipeline(GetDevice().GetPipeline(inDesc, {}));
}

void GPUGraphicsCommandList::SetViewport(const GPUViewport& inViewport)
{
    if (memcmp(&mViewport, &inViewport, sizeof(mViewport)) != 0)
    {
        mViewport = inViewport;

        mDirtyState |= kDirtyState_Viewport;
    }
}

void GPUGraphicsCommandList::SetScissor(const IntRect& inScissor)
{
    if (memcmp(&mScissor, &inScissor, sizeof(mScissor)) != 0)
    {
        mScissor = inScissor;

        mDirtyState |= kDirtyState_Scissor;
    }
}

GPUComputeCommandList::GPUComputeCommandList(GPUComputeContext&                 inContext,
                                             const GPUComputeCommandList* const inParent) :
    GPUCommandList (inContext, inParent)
{
}
