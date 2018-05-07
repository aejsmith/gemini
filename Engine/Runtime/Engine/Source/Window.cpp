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

#include "Engine/Window.h"

#include "Engine/Game.h"

#include "GPU/GPUSwapchain.h"

#include <SDL.h>

SINGLETON_IMPL(MainWindow);

Window::Window(std::string       inTitle,
               const glm::ivec2& inSize,
               const uint32_t    inFlags) :
    mSDLWindow (nullptr),
    mSwapchain (nullptr),
    mTitle     (std::move(inTitle)),
    mSize      (inSize),
    mFlags     (inFlags)
{
    uint32_t sdlFlags = 0;

    if (IsFullscreen()) sdlFlags |= SDL_WINDOW_FULLSCREEN;
    if (IsHidden())     sdlFlags |= SDL_WINDOW_HIDDEN;

    mSDLWindow = SDL_CreateWindow(mTitle.c_str(),
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  mSize.x, mSize.y,
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

void Window::SetSwapchain(GPUSwapchain* const inSwapchain,
                          OnlyCalledBy<GPUSwapchain>)
{
    Assert(!mSwapchain);
    mSwapchain = inSwapchain;
}

MainWindow::MainWindow(const glm::ivec2& inSize,
                       const uint32_t    inFlags) :
    Window (Game::Get().GetName(),
            inSize,
            inFlags)
{
}
