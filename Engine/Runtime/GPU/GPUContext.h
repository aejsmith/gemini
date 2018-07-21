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

class GPUSwapchain;

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
    virtual void                Wait(GPUContext& inOtherContext) = 0;

};

class GPUTransferContext : public GPUContext
{
protected:
                                GPUTransferContext(GPUDevice& inDevice);

public:
                                ~GPUTransferContext() {}

public:

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
     */
    virtual void                BeginPresent(GPUSwapchain& inSwapchain) = 0;
    virtual void                EndPresent(GPUSwapchain& inSwapchain) = 0;

};

class GPUGraphicsContext : public GPUComputeContext
{
protected:
                                GPUGraphicsContext(GPUDevice& inDevice);

public:
                                ~GPUGraphicsContext() {}

public:

};

inline GPUContext::GPUContext(GPUDevice& inDevice) :
    GPUDeviceChild      (inDevice)
{
}

inline GPUTransferContext::GPUTransferContext(GPUDevice& inDevice) :
    GPUContext          (inDevice)
{
}

inline GPUComputeContext::GPUComputeContext(GPUDevice& inDevice) :
    GPUTransferContext  (inDevice)
{
}

inline GPUGraphicsContext::GPUGraphicsContext(GPUDevice& inDevice) :
    GPUComputeContext  (inDevice)
{
}
