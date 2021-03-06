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

#include "GPU/GPUSwapchain.h"

#include "Engine/Window.h"

GPUSwapchain::GPUSwapchain(GPUDevice& device,
                           Window&    window) :
    GPUDeviceChild (device),
    mWindow        (window),
    mFormat        (kPixelFormat_Unknown),
    mTexture       (nullptr)
{
    mWindow.SetSwapchain(this, {});

    #if GEMINI_BUILD_DEBUG
        mIsInPresent = false;
        mViewCount.store(0, std::memory_order_release);
    #endif
}

GPUSwapchain::~GPUSwapchain()
{
    Assert(!mTexture);

    mWindow.SetSwapchain(nullptr, {});
}
