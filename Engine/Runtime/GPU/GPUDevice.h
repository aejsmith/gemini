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

#include "GPU/GPUDefs.h"

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
    /**
     * Create and attach a swapchain to the specified window so that it can be
     * rendered to. The swapchain will be permanently associated with the
     * window.
     */
    virtual void                CreateSwapchain(Window& inWindow) = 0;

    static void                 Create();

};
