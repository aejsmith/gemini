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

#include "GPU/GPUContext.h"
#include "GPU/GPUResourceView.h"
#include "GPU/GPUTexture.h"

#include "Core/Singleton.h"

class Window;

/**
 * This class is the main class of the low-level rendering API abstraction
 * layer. It provides an interface to create GPU resources and record
 * commands.
 */
class GPUDevice : public Singleton<GPUDevice>
{
protected:
                                GPUDevice();
                                ~GPUDevice();

public:
    static void                 Create();

    /** Get the primary graphics context. This is always present. */
    GPUGraphicsContext&         GetGraphicsContext() const  { return *mGraphicsContext; }

    /**
     * Create and attach a swapchain to the specified window so that it can be
     * rendered to. The swapchain will be permanently associated with the
     * window.
     */
    virtual void                CreateSwapchain(Window& inWindow) = 0;

    virtual GPUTexturePtr       CreateTexture(const GPUTextureDesc& inDesc) = 0;

    virtual GPUResourceViewPtr  CreateResourceView(GPUResource&               inResource,
                                                   const GPUResourceViewDesc& inDesc) = 0;

    /**
     * Mark the end of a frame. Should be called after the last work submission
     * in the frame (usually an EndPresent()). Any recorded work that has not
     * yet been submitted to the device on each context will be submitted.
     * This function can block waiting for GPU work from earlier frames to
     * complete, so that the CPU does not get too far ahead of the GPU.
     */
    virtual void                EndFrame() = 0;

protected:
    GPUGraphicsContext*         mGraphicsContext;

};
