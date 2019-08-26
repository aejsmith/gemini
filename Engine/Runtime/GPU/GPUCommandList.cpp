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

#include "GPU/GPUCommandList.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUConstantPool.h"
#include "GPU/GPUContext.h"

GPUCommandList::GPUCommandList(GPUContext&                 inContext,
                               const GPUCommandList* const inParent) :
    GPUDeviceChild  (inContext.GetDevice()),
    mContext        (inContext),
    mParent         (inParent),
    mState          (kState_Created)
{
    memset(mArgumentState, 0, sizeof(mArgumentState));
    for (auto& argumentState : mArgumentState)
    {
        argumentState.dirty = true;

        #if GEMINI_BUILD_DEBUG
            argumentState.valid = false;

            for (auto& constants : argumentState.constants)
            {
                constants = kGPUConstants_Invalid;
            }
        #endif
    }

    #if GEMINI_BUILD_DEBUG
        mActiveChildCount.store(0, std::memory_order_relaxed);
    #endif
}

void GPUCommandList::SetArguments(const uint8_t         inIndex,
                                  GPUArgumentSet* const inSet)
{
    Assert(inIndex < kMaxArgumentSets);
    Assert(inSet);
    Assert(inSet->GetLayout() == mArgumentState[inIndex].layout);

    SetArgumentsImpl(inIndex, inSet);

    #if GEMINI_BUILD_DEBUG
        mArgumentState[inIndex].valid = true;
    #endif
}

void GPUCommandList::SetArguments(const uint8_t            inIndex,
                                  const GPUArgument* const inArguments)
{
    auto& argumentState = mArgumentState[inIndex];

    Assert(inIndex < kMaxArgumentSets);
    Assert(argumentState.layout);

    GPUArgumentSet::ValidateArguments(argumentState.layout, inArguments);

    SetArgumentsImpl(inIndex, inArguments);

    #if GEMINI_BUILD_DEBUG
        mArgumentState[inIndex].valid = true;
    #endif
}

void GPUCommandList::SetConstants(const uint8_t      inSetIndex,
                                  const uint8_t      inArgumentIndex,
                                  const GPUConstants inConstants)
{
    auto& argumentState = mArgumentState[inSetIndex];

    Assert(inSetIndex < kMaxArgumentSets);
    Assert(argumentState.layout);
    Assert(inArgumentIndex < argumentState.layout->GetArgumentCount());
    Assert(argumentState.layout->GetArguments()[inArgumentIndex] == kGPUArgumentType_Constants);

    if (argumentState.constants[inArgumentIndex] != inConstants)
    {
        argumentState.constants[inArgumentIndex] = inConstants;
        argumentState.dirty                      = true;
    }
}

void GPUCommandList::ChangeArgumentLayout(const GPUArgumentSetLayoutRef (&inLayouts)[kMaxArgumentSets])
{
    for (size_t setIndex = 0; setIndex < kMaxArgumentSets; setIndex++)
    {
        auto& argumentState = mArgumentState[setIndex];

        const GPUArgumentSetLayoutRef layout = inLayouts[setIndex];

        if (layout != argumentState.layout)
        {
            argumentState.layout = layout;
            argumentState.dirty  = true;

            if (layout->IsConstantOnly())
            {
                SetArgumentsImpl(setIndex, static_cast<const GPUArgument*>(nullptr));
            }

            #if GEMINI_BUILD_DEBUG
                argumentState.valid = layout->IsConstantOnly();

                for (auto& constants : argumentState.constants)
                {
                    constants = kGPUConstants_Invalid;
                }
            #endif
        }
    }
}

#ifdef GEMINI_BUILD_DEBUG

void GPUCommandList::ValidateArguments() const
{
    for (size_t setIndex = 0; setIndex < kMaxArgumentSets; setIndex++)
    {
        auto& argumentState = mArgumentState[setIndex];

        if (argumentState.layout)
        {
            Assert(argumentState.valid);

            for (uint8_t argumentIndex = 0; argumentIndex < argumentState.layout->GetArgumentCount(); argumentIndex++)
            {
                if (argumentState.layout->GetArguments()[argumentIndex] == kGPUArgumentType_Constants)
                {
                    Assert(argumentState.constants[argumentIndex] != kGPUConstants_Invalid);
                }
            }
        }
    }
}

#endif

void GPUCommandList::WriteConstants(const uint8_t     inSetIndex,
                                    const uint8_t     inArgumentIndex,
                                    const void* const inData,
                                    const size_t      inSize)
{
    auto& argumentState = mArgumentState[inSetIndex];

    Assert(inSetIndex < kMaxArgumentSets);
    Assert(argumentState.layout);
    Assert(inArgumentIndex < argumentState.layout->GetArgumentCount());
    Assert(argumentState.layout->GetArguments()[inArgumentIndex] == kGPUArgumentType_Constants);

    argumentState.constants[inArgumentIndex] = GetDevice().GetConstantPool().Write(inData, inSize);
    argumentState.dirty                      = true;
}

GPUComputeCommandList::GPUComputeCommandList(GPUComputeContext&                 inContext,
                                             const GPUComputeCommandList* const inParent) :
    GPUCommandList  (inContext, inParent),
    mPipeline       (nullptr),
    mPipelineDirty  (false)
{
}

GPUComputeCommandList::~GPUComputeCommandList()
{
}

void GPUComputeCommandList::SetPipeline(GPUComputePipeline* const inPipeline)
{
    Assert(inPipeline);

    if (inPipeline != mPipeline)
    {
        mPipeline = inPipeline;

        ChangeArgumentLayout(inPipeline->GetDesc().argumentSetLayouts);

        mPipelineDirty = true;
    }
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
}

void GPUGraphicsCommandList::SetPipeline(GPUPipeline* const inPipeline)
{
    Assert(inPipeline);

    if (inPipeline != mPipeline)
    {
        mPipeline = inPipeline;

        ChangeArgumentLayout(inPipeline->GetDesc().argumentSetLayouts);

        mDirtyState |= kDirtyState_Pipeline;
    }
}

void GPUGraphicsCommandList::SetPipeline(const GPUPipelineDesc& inDesc)
{
    SetPipeline(GetDevice().GetPipeline(inDesc));
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

void GPUGraphicsCommandList::SetVertexBuffer(const uint32_t   inIndex,
                                             GPUBuffer* const inBuffer,
                                             const uint32_t   inOffset)
{
    Assert(inIndex < kMaxVertexAttributes);

    auto& vertexBuffer = mVertexBuffers[inIndex];

    if (inBuffer != vertexBuffer.buffer || inOffset != vertexBuffer.offset)
    {
        vertexBuffer.buffer = inBuffer;
        vertexBuffer.offset = inOffset;

        mDirtyVertexBuffers.set(inIndex);
    }
}

void GPUGraphicsCommandList::SetIndexBuffer(const GPUIndexType inType,
                                            GPUBuffer* const   inBuffer,
                                            const uint32_t     inOffset)
{
    if (inType != mIndexBuffer.type || inBuffer != mIndexBuffer.buffer || inOffset != mIndexBuffer.offset)
    {
        mIndexBuffer.type   = inType;
        mIndexBuffer.buffer = inBuffer;
        mIndexBuffer.offset = inOffset;

        mDirtyState |= kDirtyState_IndexBuffer;
    }
}

void GPUGraphicsCommandList::WriteVertexBuffer(const uint32_t    inIndex,
                                               const void* const inData,
                                               const size_t      inSize)
{
    Assert(inIndex < kMaxVertexAttributes);

    auto& vertexBuffer = mVertexBuffers[inIndex];

    void* mapping;

    vertexBuffer.buffer = nullptr;
    vertexBuffer.offset = AllocateTransientBuffer(inSize, mapping);

    memcpy(mapping, inData, inSize);

    mDirtyVertexBuffers.set(inIndex);
}

void GPUGraphicsCommandList::WriteIndexBuffer(const GPUIndexType inType,
                                              const void* const  inData,
                                              const size_t       inSize)
{
    void* mapping;

    mIndexBuffer.type   = inType;
    mIndexBuffer.buffer = nullptr;
    mIndexBuffer.offset = AllocateTransientBuffer(inSize, mapping);

    memcpy(mapping, inData, inSize);

    mDirtyState |= kDirtyState_IndexBuffer;
}
