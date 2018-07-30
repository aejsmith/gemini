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

#include "GPU/GPUDeviceChild.h"
#include "GPU/GPURenderPass.h"

#include <atomic>
#include <thread>

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
                                GPUCommandList(GPUDevice& inDevice);
                                ~GPUCommandList() {}

public:
    /**
     * Begin the command list. This must be called before recording any
     * commands. This ties the command list to the calling thread. The reason
     * why this must be done explicitly is to enable a command list to be
     * created from one thread (usually the main thread), and then passed off
     * to another thread to do the actual work.
     */
    void                        Begin();

    /**
     * End the command list. After this is called, no more commands can be
     * recorded. This must be called before submitting the command list to its
     * parent.
     */
    void                        End();

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
    GPUCommandList*             CreateChild();

    /**
     * Submit an array of child command lists. These will be ordered after any
     * commands previously recorded within this command list, and the commands
     * within the children will be performed in the order in which they are
     * found in the array.
     */
    void                        SubmitChildren(GPUCommandList** const inChildren,
                                               const uint32_t         inCount);

protected:
    enum State
    {
        kState_Created,
        kState_Begun,
        kState_Ended,
    };

protected:
    /**
     * Validate that the command list is in the correct state and that it is
     * being used from the correct thread.
     */
    void                        ValidateCommand() const;

    virtual void                BeginImpl() {}
    virtual void                EndImpl() {}

    virtual GPUCommandList*     CreateChildImpl() = 0;

    virtual void                SubmitChildrenImpl(GPUCommandList** const inChildren,
                                                   const uint32_t         inCount) = 0;

protected:
    /** State of the command list. */
    State                       mState;

    #if ORION_BUILD_DEBUG

    std::thread::id             mOwningThread;
    std::atomic<uint32_t>       mActiveChildCount;

    #endif
};

class GPUGraphicsCommandList : public GPUCommandList
{
protected:
                                GPUGraphicsCommandList(GPUDevice& inDevice);
                                ~GPUGraphicsCommandList() {}

public:
    /**
     * See GPUCommandList::CreateChild(). This is the same but returns a
     * GPUGraphicsCommandList instead.
     */
    GPUGraphicsCommandList*     CreateChild()
                                    { return static_cast<GPUGraphicsCommandList*>(GPUCommandList::CreateChild()); }

};

class GPUComputeCommandList : public GPUCommandList
{
protected:
                                GPUComputeCommandList(GPUDevice& inDevice);
                                ~GPUComputeCommandList() {}

public:
    /**
     * See GPUCommandList::CreateChild(). This is the same but returns a
     * GPUComputeCommandList instead.
     */
    GPUComputeCommandList*      CreateChild()
                                    { return static_cast<GPUComputeCommandList*>(GPUCommandList::CreateChild()); }

};

inline void GPUCommandList::Begin()
{
    Assert(mState == kState_Created);

    BeginImpl();
    mState = kState_Begun;
}

inline void GPUCommandList::End()
{
    Assert(mState == kState_Begun);
    Assert(mActiveChildCount.load(std::memory_order_relaxed) == 0);

    EndImpl();
    mState = kState_Ended;
}

inline void GPUCommandList::ValidateCommand() const
{
    Assert(mState == kState_Begun);
    Assert(mOwningThread == std::this_thread::get_id());
}

inline GPUCommandList* GPUCommandList::CreateChild()
{
    #if ORION_BUILD_DEBUG
        mActiveChildCount.fetch_add(1, std::memory_order_relaxed);
    #endif

    return CreateChildImpl();
}

inline void GPUCommandList::SubmitChildren(GPUCommandList** const inChildren,
                                           const uint32_t         inCount)
{
    #if ORION_BUILD_DEBUG
        mActiveChildCount.fetch_sub(inCount, std::memory_order_relaxed);
    #endif

    SubmitChildrenImpl(inChildren, inCount);
}
