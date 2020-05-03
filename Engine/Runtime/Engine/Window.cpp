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

#include "Engine/Window.h"

#include "Engine/Game.h"

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUSwapchain.h"

#include <SDL.h>

SINGLETON_IMPL(MainWindow);

Window::Window(std::string       title,
               const glm::uvec2& size,
               const uint32_t    flags) :
    RenderOutput    (size),
    mSDLWindow      (nullptr),
    mSwapchain      (nullptr),
    mTitle          (std::move(title)),
    mFlags          (flags)
{
    uint32_t sdlFlags = 0;

    if (IsFullscreen()) sdlFlags |= SDL_WINDOW_FULLSCREEN;
    if (IsHidden())     sdlFlags |= SDL_WINDOW_HIDDEN;

    mSDLWindow = SDL_CreateWindow(mTitle.c_str(),
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  GetSize().x, GetSize().y,
                                  sdlFlags);
    if (!mSDLWindow)
    {
        Fatal("Failed to create window: %s", SDL_GetError());
    }
}

Window::~Window()
{
    delete mSwapchain;

    SDL_DestroyWindow(mSDLWindow);
}

void Window::SetSwapchain(GPUSwapchain* const swapchain,
                          OnlyCalledBy<GPUSwapchain>)
{
    Assert(!swapchain || !mSwapchain);

    mSwapchain = swapchain;

    if (mSwapchain)
    {
        RegisterOutput();
    }
    else
    {
        UnregisterOutput();
    }
}

GPUTexture* Window::GetTexture() const
{
    Assert(mSwapchain);
    return mSwapchain->GetTexture();
}

std::string Window::GetName() const
{
    return std::string("Window '") + mTitle + std::string("'");
}

GPUResourceState Window::GetFinalState() const
{
    return kGPUResourceState_Present;
}

void Window::BeginRender()
{
    Assert(mSwapchain);
    GPUGraphicsContext::Get().BeginPresent(*mSwapchain);
}

void Window::EndRender()
{
    Assert(mSwapchain);
    GPUGraphicsContext::Get().EndPresent(*mSwapchain);
}

MainWindow::MainWindow(const glm::uvec2& size,
                       const uint32_t    flags) :
    Window (Game::Get().GetTitle(),
            size,
            flags)
{
}

void MainWindow::EndRender()
{
    /* We'll defer this to the very end of the frame rather than doing it as
     * soon as the render graph is done with us, so that Engine can draw debug
     * UI as late as possible in the frame. */
}

void MainWindow::Present(OnlyCalledBy<Engine>)
{
    Window::EndRender();
}
