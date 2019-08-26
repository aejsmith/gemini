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

#include "Core/PixelFormat.h"
#include "Core/Singleton.h"

#include "GPU/GPUResourceView.h"
#include "GPU/GPUTexture.h"

#include <atomic>

class Window;

/**
 * Interface between the GPU layer and Window. A window needs to have a
 * GPUSwapchain to be able to render to it.
 */
class GPUSwapchain : public GPUDeviceChild
{
protected:
                                GPUSwapchain(GPUDevice& inDevice,
                                             Window&    inWindow);

public:
    virtual                     ~GPUSwapchain();

public:
    Window&                     GetWindow() const { return mWindow; }
    PixelFormat                 GetFormat() const { return mFormat; }

    /**
     * Get a texture referring to the swapchain. This is a special texture
     * which has restricted usage.
     *
     * It is only valid to use the texture between calls to
     * GPUComputeContext::BeginPresent() and GPUComputeContext::EndPresent().
     * This is because the backend may need to explicitly acquire a new texture
     * to use from the window system, and also insert synchronisation around
     * its usage. This restriction therefore allows these steps to be done only
     * around where they really need to be. Usage of the texture must then
     * occur only on the context where BeginPresent() was called.
     *
     * Views to the texture must be created each frame, after BeginPresent() is
     * called, so that they can be made to refer to correct texture for the
     * frame. Views must be destroyed before EndPresent() is called.
     */
    GPUTexture*                 GetTexture() const  { return mTexture; }

protected:
    void                        OnBeginPresent();
    void                        OnEndPresent();

protected:
    Window&                     mWindow;
    PixelFormat                 mFormat;
    GPUTexture*                 mTexture;

    #if GEMINI_BUILD_DEBUG

    bool                        mIsInPresent;

    /**
     * Count of views referring to the swapchain to validate that no views
     * exist outside BeginPresent()/EndPresent().
     */
    std::atomic<uint32_t>       mViewCount;

    #endif

    friend class GPUResourceView;
};

inline void GPUSwapchain::OnBeginPresent()
{
    #if GEMINI_BUILD_DEBUG
        Assert(!mIsInPresent);
        mIsInPresent = true;
    #endif
}

inline void GPUSwapchain::OnEndPresent()
{
    #if GEMINI_BUILD_DEBUG
        Assert(mIsInPresent);
        AssertMsg(mViewCount.load(std::memory_order_relaxed) == 0,
                  "Swapchain views still exist at call to EndPresent()");

        mIsInPresent = false;
    #endif
}