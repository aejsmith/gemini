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

#include "Core/Thread.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPURenderPass.h"

#include <atomic>

class GPUBuffer;
class GPUContext;
class GPUComputeContext;
class GPUComputePipeline;
class GPUGraphicsContext;
class GPUPipeline;

struct GPUPipelineDesc;

/**
 * This class and its derived classes provide the interface for recording
 * commands within a render or compute pass (i.e. draw/dispatch calls). Usage
 * is as follows:
 *
 *   1. Create a command list, through Create*Pass() on a GPUContext, or
 *      through CreateChild().
 *   2. Call Begin().
 *   3. Record some commands.
 *   4. Call End(). No more commands can be recorded after this point.
 *   5. Submit the command list back to its parent (context or command list).
 *
 * Command lists are transient objects. Once they have been submitted to the
 * parent it is no longer safe to access the object. They also cannot live
 * across a frame boundary.
 *
 * Command lists enable multithreaded command recording. Individual command
 * lists can only be recorded from a single thread (they are tied to a thread
 * from the point where Begin() is called), however multithreaded recording
 * within a pass can be achieved through the use of child command lists.
 *
 * Example:
 *
 *   Main thread:
 *     GPUGraphicsCommandList* baseCmdList =
 *         GPUGraphicsContext::Get().CreateRenderPass(...);
 *     baseCmdList->Begin();
 *
 *     std::vector<GPUCommandList*> cmdLists(<number of lists>);
 *
 *     for (<some division of work between threads>)
 *     {
 *         GPUGraphicsCommandList* cmdList = baseCmdList->CreateChild();
 *         <pass cmdList and some work off to a worker>
 *         cmdLists.emplace_back(cmdList);
 *     }
 *
 *     baseCmdList->SubmitChildren(cmdLists.data(), cmdLists.size());
 *     baseCmdList->End();
 *
 *     GPUGraphicsContext::Get().SubmitRenderPass(baseCmdList);
 *
 *   Worker threads:
 *     GPUGraphicsCommandList* cmdList = <passed from main thread>;
 *     cmdList->Begin();
 *     <submit commands>
 *     cmdList->End();
 *
 * See documentation for each of the methods used in the example for more
 * details.
 */
class GPUCommandList : public GPUDeviceChild
{
protected:
                                    GPUCommandList(GPUContext&                 inContext,
                                                   const GPUCommandList* const inParent);
                                    ~GPUCommandList() {}

public:
    enum State
    {
        kState_Created,
        kState_Begun,
        kState_Ended,
    };

public:
    GPUContext&                     GetContext() const  { return mContext; }
    const GPUCommandList*           GetParent() const   { return mParent; }
    State                           GetState() const    { return mState; }

    /**
     * Begin the command list. This must be called before recording any
     * commands. This ties the command list to the calling thread. The reason
     * why this must be done explicitly is to enable a command list to be
     * created from one thread (usually the main thread), and then passed off
     * to another thread to do the actual work.
     */
    void                            Begin();

    /**
     * End the command list. After this is called, no more commands can be
     * recorded. This must be called before submitting the command list to its
     * parent.
     */
    void                            End();

    /**
     * Create a new child command list. A child command list is entirely
     * independent of its parent . The order in which this is called does not
     * determine the order in which child command lists are submitted - this
     * is defined only by the order in which SubmitChildren() is called.
     *
     * This method can be called on any thread, and can also be called before
     * Begin() has been called. It is the only method in this class for which
     * this is the case.
     */
    GPUCommandList*                 CreateChild();

    /**
     * Submit an array of child command lists. These will be ordered after any
     * commands previously recorded within this command list, and the commands
     * within the children will be performed in the order in which they are
     * found in the array.
     */
    void                            SubmitChildren(GPUCommandList** const inChildren,
                                                   const size_t           inCount);

    /**
     * Set shader arguments to be used for subsequent draw/dispatch commands.
     * The overload taking a GPUArgumentSet pointer binds a pre-existing
     * argument set. This set's layout must match the layout specified in the
     * currently bound pipeline or compute shader for the given set index.
     *
     * The other dynamically allocates a temporary argument set binding the
     * specified arguments, which again must match the pipeline's layout for
     * the given set index. Argument array follows the same rules as for
     * GPUDevice::CreateArgumentSet(). See GPUArgumentSet for more details of
     * persistent vs. dynamically created sets.
     *
     * When binding a pipeline, any set index which uses a constant-only layout
     * will automatically have valid arguments bound, there is no need to call
     * these functions. It is only necessary to set the constants themselves.
     *
     * Any arguments of type kGPUArgumentType_Constants in the set are initially
     * invalid. Before drawing, SetConstants()/WriteConstants() must be used to
     * set constant data written in the current frame.
     *
     * See GPUGraphicsCommandList::SetPipeline() for details of how changing
     * pipeline affects bound argument state.
     */
    void                            SetArguments(const uint8_t         inIndex,
                                                 GPUArgumentSet* const inSet);
    void                            SetArguments(const uint8_t            inIndex,
                                                 const GPUArgument* const inArguments);

    /**
     * Set data for a kGPUArgumentType_Constants shader argument. This remains
     * valid until the argument set layout at the given set index changes (i.e.
     * due to a pipeline change).
     */
    void                            SetConstants(const uint8_t     inSetIndex,
                                                 const uint8_t     inArgumentIndex,
                                                 const GPUConstants inConstants);

    /**
     * Convenience function which writes new data to the constant pool and then
     * sets it with SetConstants().
     */
    void                            WriteConstants(const uint8_t     inSetIndex,
                                                   const uint8_t     inArgumentIndex,
                                                   const void* const inData,
                                                   const size_t      inSize);

protected:
    struct ArgumentState
    {
        /** Layout as expected by the pipeline/compute shader. */
        GPUArgumentSetLayoutRef     layout;

        /** Currently set constant handles. */
        GPUConstants                constants[kMaxArgumentsPerSet];

        /**
         * Dirty state tracking for the backend. Set when the layout, set or
         * constants change.
         */
        bool                        dirty : 1;

        #if ORION_BUILD_DEBUG
        bool                        valid : 1;
        #endif
    };

protected:
    void                            ChangeArgumentLayout(const GPUArgumentSetLayoutRef (&inLayouts)[kMaxArgumentSets]);

    /**
     * Validate that the command list is in the correct state and that it is
     * being used from the correct thread.
     */
    void                            ValidateCommand() const;

    void                            ValidateArguments() const;

    virtual void                    BeginImpl() {}
    virtual void                    EndImpl() {}

    virtual GPUCommandList*         CreateChildImpl() = 0;

    virtual void                    SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                       const size_t           inCount) = 0;

    /**
     * Set shader arguments. Responsible for setting backend-specific state on
     * the command list, dynamically allocating it in the second case, and for
     * flagging argument state as dirty if the backend determines it is
     * necessary.
     */
    virtual void                    SetArgumentsImpl(const uint8_t         inIndex,
                                                     GPUArgumentSet* const inSet) = 0;
    virtual void                    SetArgumentsImpl(const uint8_t            inIndex,
                                                     const GPUArgument* const inArguments) = 0;

protected:
    GPUContext&                     mContext;
    const GPUCommandList* const     mParent;

    /** State of the command list. */
    State                           mState;

    /** Bound shader argument state. */
    ArgumentState                   mArgumentState[kMaxArgumentSets];

    #if ORION_BUILD_DEBUG

    ThreadID                        mOwningThread;
    std::atomic<size_t>             mActiveChildCount;

    #endif
};

class GPUComputeCommandList : public GPUCommandList
{
protected:
                                    GPUComputeCommandList(GPUComputeContext&                 inContext,
                                                          const GPUComputeCommandList* const inParent);
                                    ~GPUComputeCommandList();

public:
    /**
     * See GPUCommandList::CreateChild(). This is the same but returns a
     * GPUComputeCommandList instead.
     */
    GPUComputeCommandList*          CreateChild()
                                        { return static_cast<GPUComputeCommandList*>(GPUCommandList::CreateChild()); }

    /**
     * Set the compute pipeline to use for subsequent dispatches.
     *
     * For each argument set index, if the new pipeline's layout at that index
     * differs from the old pipeline's, then any bound arguments at that index
     * will be unbound. Otherwise, bound arguments will remain bound.
     */
    void                            SetPipeline(GPUComputePipeline* const inPipeline);

    virtual void                    Dispatch(const uint32_t inGroupCountX,
                                             const uint32_t inGroupCountY,
                                             const uint32_t inGroupCountZ) = 0;

protected:
    GPUComputePipeline*             mPipeline;

    bool                            mPipelineDirty : 1;

};

class GPUGraphicsCommandList : public GPUCommandList
{
protected:
                                    GPUGraphicsCommandList(GPUGraphicsContext&                 inContext,
                                                           const GPUGraphicsCommandList* const inParent,
                                                           const GPURenderPass&                inRenderPass);
                                    ~GPUGraphicsCommandList();

public:
    const GPURenderPass&            GetRenderPass() const           { return mRenderPass; }
    GPURenderTargetStateRef         GetRenderTargetState() const    { return mRenderTargetState; }

    GPUResourceView*                GetColourView(const uint32_t inIndex) const
                                        { return mRenderPass.colour[inIndex].view; }
    GPUResourceView*                GetDepthStencilView() const
                                        { return mRenderPass.depthStencil.view; }

    /**
     * See GPUCommandList::CreateChild(). This is the same but returns a
     * GPUGraphicsCommandList instead.
     */
    GPUGraphicsCommandList*         CreateChild()
                                        { return static_cast<GPUGraphicsCommandList*>(GPUCommandList::CreateChild()); }

    /**
     * Set the pipeline to use for subsequent draws, to either a pre-created
     * pipeline object, or to a dynamically created pipeline matching the
     * specified state.
     *
     * For each argument set index, if the new pipeline's layout at that index
     * differs from the old pipeline's, then any bound arguments at that index
     * will be unbound. Otherwise, bound arguments will remain bound.
     */
    void                            SetPipeline(GPUPipeline* const inPipeline);
    void                            SetPipeline(const GPUPipelineDesc& inDesc);

    void                            SetViewport(const GPUViewport& inViewport);
    void                            SetScissor(const IntRect& inScissor);

    void                            SetVertexBuffer(const uint32_t   inIndex,
                                                    GPUBuffer* const inBuffer,
                                                    const uint32_t   inOffset = 0);

    void                            SetIndexBuffer(const GPUIndexType inType,
                                                   GPUBuffer* const   inBuffer,
                                                   const uint32_t     inOffset = 0);

    virtual void                    Draw(const uint32_t inVertexCount,
                                         const uint32_t inFirstVertex = 0) = 0;

    virtual void                    DrawIndexed(const uint32_t inIndexCount,
                                                const uint32_t inFirstIndex  = 0,
                                                const int32_t  inVertexOffset = 0) = 0;

protected:
    enum DirtyState
    {
        kDirtyState_Pipeline    = (1 << 0),
        kDirtyState_Viewport    = (1 << 1),
        kDirtyState_Scissor     = (1 << 2),
        kDirtyState_IndexBuffer = (1 << 3),

        kDirtyState_All         = kDirtyState_Pipeline |
                                  kDirtyState_Viewport |
                                  kDirtyState_Scissor |
                                  kDirtyState_IndexBuffer
    };

    struct VertexBuffer
    {
        GPUBuffer*                  buffer;
        uint32_t                    offset;
        bool                        dirty : 1;
    };

    struct IndexBuffer
    {
        GPUIndexType                type;
        GPUBuffer*                  buffer;
        uint32_t                    offset;
    };

protected:
    const GPURenderPass             mRenderPass;
    const GPURenderTargetStateRef   mRenderTargetState;

    uint32_t                        mDirtyState;

    GPUPipeline*                    mPipeline;
    GPUViewport                     mViewport;
    IntRect                         mScissor;
    VertexBuffer                    mVertexBuffers[kMaxVertexAttributes];
    IndexBuffer                     mIndexBuffer;

};

inline void GPUCommandList::Begin()
{
    Assert(mState == kState_Created);

    BeginImpl();
    mState = kState_Begun;

    #if ORION_BUILD_DEBUG
        mOwningThread = Thread::GetCurrentID();
    #endif
}

inline void GPUCommandList::End()
{
    ValidateCommand();
    Assert(mActiveChildCount.load(std::memory_order_relaxed) == 0);

    EndImpl();
    mState = kState_Ended;
}

inline void GPUCommandList::ValidateCommand() const
{
    Assert(mState == kState_Begun);
    Assert(mOwningThread == Thread::GetCurrentID());
}

#ifndef ORION_BUILD_DEBUG

inline void GPUCommandList::ValidateArguments() const
{
    /* No validation on non-debug builds. */
}

#endif

inline GPUCommandList* GPUCommandList::CreateChild()
{
    #if ORION_BUILD_DEBUG
        mActiveChildCount.fetch_add(1, std::memory_order_relaxed);
    #endif

    return CreateChildImpl();
}

inline void GPUCommandList::SubmitChildren(GPUCommandList** const inChildren,
                                           const size_t           inCount)
{
    ValidateCommand();

    #if ORION_BUILD_DEBUG
        for (size_t i = 0; i < inCount; i++)
        {
            Assert(inChildren[i]->GetState() == GPUCommandList::kState_Ended);
        }

        mActiveChildCount.fetch_sub(inCount, std::memory_order_relaxed);
    #endif

    SubmitChildrenImpl(inChildren, inCount);
}
