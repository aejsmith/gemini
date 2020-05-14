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

#include "GPU/GPUCommandList.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUConstantPool.h"
#include "GPU/GPUContext.h"

GPUCommandList::GPUCommandList(GPUContext&                 context,
                               const GPUCommandList* const parent) :
    GPUDeviceChild  (context.GetDevice()),
    mContext        (context),
    mParent         (parent),
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

void GPUCommandList::SetArguments(const uint8_t         index,
                                  GPUArgumentSet* const set)
{
    Assert(index < kMaxArgumentSets);
    Assert(set);
    Assert(set->GetLayout() == mArgumentState[index].layout);

    SetArgumentsImpl(index, set);

    #if GEMINI_BUILD_DEBUG
        mArgumentState[index].valid = true;
    #endif
}

void GPUCommandList::SetArguments(const uint8_t            index,
                                  const GPUArgument* const arguments)
{
    auto& argumentState = mArgumentState[index];

    Assert(index < kMaxArgumentSets);
    Assert(argumentState.layout);

    GPUArgumentSet::ValidateArguments(argumentState.layout, arguments);

    SetArgumentsImpl(index, arguments);

    #if GEMINI_BUILD_DEBUG
        mArgumentState[index].valid = true;
    #endif
}

void GPUCommandList::SetConstants(const uint8_t      setIndex,
                                  const uint8_t      argumentIndex,
                                  const GPUConstants constants)
{
    auto& argumentState = mArgumentState[setIndex];

    Assert(setIndex < kMaxArgumentSets);
    Assert(argumentState.layout);
    Assert(argumentIndex < argumentState.layout->GetArgumentCount());
    Assert(argumentState.layout->GetArguments()[argumentIndex] == kGPUArgumentType_Constants);

    if (argumentState.constants[argumentIndex] != constants)
    {
        argumentState.constants[argumentIndex] = constants;
        argumentState.dirty                    = true;
    }
}

void GPUCommandList::ChangeArgumentLayout(const GPUArgumentSetLayoutRef (&layouts)[kMaxArgumentSets])
{
    for (size_t setIndex = 0; setIndex < kMaxArgumentSets; setIndex++)
    {
        auto& argumentState = mArgumentState[setIndex];

        const GPUArgumentSetLayoutRef layout = layouts[setIndex];

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

#if GEMINI_BUILD_DEBUG

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

void GPUCommandList::WriteConstants(const uint8_t     setIndex,
                                    const uint8_t     argumentIndex,
                                    const void* const data,
                                    const size_t      size)
{
    auto& argumentState = mArgumentState[setIndex];

    Assert(setIndex < kMaxArgumentSets);
    Assert(argumentState.layout);
    Assert(argumentIndex < argumentState.layout->GetArgumentCount());
    Assert(argumentState.layout->GetArguments()[argumentIndex] == kGPUArgumentType_Constants);

    argumentState.constants[argumentIndex] = GetDevice().GetConstantPool().Write(data, size);
    argumentState.dirty                    = true;
}

GPUComputeCommandList::GPUComputeCommandList(GPUComputeContext&                 context,
                                             const GPUComputeCommandList* const parent) :
    GPUCommandList  (context, parent),
    mPipeline       (nullptr),
    mPipelineDirty  (false)
{
}

GPUComputeCommandList::~GPUComputeCommandList()
{
}

void GPUComputeCommandList::SetPipeline(GPUComputePipeline* const pipeline)
{
    Assert(pipeline);

    if (pipeline != mPipeline)
    {
        mPipeline = pipeline;

        ChangeArgumentLayout(pipeline->GetDesc().argumentSetLayouts);

        mPipelineDirty = true;
    }
}

GPUGraphicsCommandList::GPUGraphicsCommandList(GPUGraphicsContext&                 context,
                                               const GPUGraphicsCommandList* const parent,
                                               const GPURenderPass&                renderPass) :
    GPUCommandList      (context, parent),
    mRenderPass         (renderPass),
    mRenderTargetState  ((parent)
                             ? parent->GetRenderTargetState()
                             : mRenderPass.GetRenderTargetState()),
    mDirtyState         (0),
    mPipeline           (nullptr)
{
    /* Initialise the viewport and scissor to the size of the render target. */
    uint32_t width, height, layers;
    renderPass.GetDimensions(width, height, layers);

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

void GPUGraphicsCommandList::SetPipeline(const GPUPipelineRef pipeline)
{
    Assert(pipeline);

    if (pipeline != mPipeline)
    {
        mPipeline = pipeline;

        ChangeArgumentLayout(pipeline->GetDesc().argumentSetLayouts);

        mDirtyState |= kDirtyState_Pipeline;
    }
}

void GPUGraphicsCommandList::SetPipeline(const GPUPipelineDesc& desc)
{
    SetPipeline(GetDevice().GetPipeline(desc));
}

void GPUGraphicsCommandList::SetViewport(const GPUViewport& viewport)
{
    if (memcmp(&mViewport, &viewport, sizeof(mViewport)) != 0)
    {
        mViewport = viewport;

        mDirtyState |= kDirtyState_Viewport;
    }
}

void GPUGraphicsCommandList::SetScissor(const IntRect& scissor)
{
    if (memcmp(&mScissor, &scissor, sizeof(mScissor)) != 0)
    {
        mScissor = scissor;

        mDirtyState |= kDirtyState_Scissor;
    }
}

void GPUGraphicsCommandList::SetVertexBuffer(const uint32_t   index,
                                             GPUBuffer* const buffer,
                                             const uint32_t   offset)
{
    Assert(index < kMaxVertexAttributes);

    auto& vertexBuffer = mVertexBuffers[index];

    if (buffer != vertexBuffer.buffer || offset != vertexBuffer.offset)
    {
        vertexBuffer.buffer = buffer;
        vertexBuffer.offset = offset;

        mDirtyVertexBuffers.Set(index);
    }
}

void GPUGraphicsCommandList::SetIndexBuffer(const GPUIndexType type,
                                            GPUBuffer* const   buffer,
                                            const uint32_t     offset)
{
    if (type != mIndexBuffer.type || buffer != mIndexBuffer.buffer || offset != mIndexBuffer.offset)
    {
        mIndexBuffer.type   = type;
        mIndexBuffer.buffer = buffer;
        mIndexBuffer.offset = offset;

        mDirtyState |= kDirtyState_IndexBuffer;
    }
}

void GPUGraphicsCommandList::WriteVertexBuffer(const uint32_t    index,
                                               const void* const data,
                                               const size_t      size)
{
    Assert(index < kMaxVertexAttributes);

    auto& vertexBuffer = mVertexBuffers[index];

    void* mapping;

    vertexBuffer.buffer = nullptr;
    vertexBuffer.offset = AllocateTransientBuffer(size, mapping);

    memcpy(mapping, data, size);

    mDirtyVertexBuffers.Set(index);
}

void GPUGraphicsCommandList::WriteIndexBuffer(const GPUIndexType type,
                                              const void* const  data,
                                              const size_t       size)
{
    void* mapping;

    mIndexBuffer.type   = type;
    mIndexBuffer.buffer = nullptr;
    mIndexBuffer.offset = AllocateTransientBuffer(size, mapping);

    memcpy(mapping, data, size);

    mDirtyState |= kDirtyState_IndexBuffer;
}
