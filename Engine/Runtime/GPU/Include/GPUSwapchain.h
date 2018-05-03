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

#include "Core/PixelFormat.h"
#include "Core/Singleton.h"

#include "GPU/GPUDeviceChild.h"

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

protected:
    Window&                     mWindow;
    PixelFormat                 mFormat;

};
