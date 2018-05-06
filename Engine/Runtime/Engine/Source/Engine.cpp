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

#include "Engine/Engine.h"

#include "Core/Filesystem.h"
#include "Core/String.h"

#include "Engine/Window.h"

#include "GPU/GPUDevice.h"

#include <SDL.h>

SINGLETON_IMPL(Engine);

Engine::Engine()
{
    LogInfo("Hello, World!");

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        Fatal("Failed to initialize SDL: %s", SDL_GetError());
    }

    /* Find the engine base directory and switch to it. */
    char* const platformBasePath = SDL_GetBasePath();
    Path basePath(platformBasePath, Path::kUnnormalizedPlatform);
    basePath /= "../..";

    if (!Filesystem::SetWorkingDirectory(basePath))
    {
        Fatal("Failed to change to engine directory '%s'", basePath.GetCString());
    }

    SDL_free(platformBasePath);

    /* Set up the main window and graphics API. TODO: Make parameters
     * configurable. */
    new MainWindow(glm::ivec2(1600, 900), 0);
    GPUDevice::Create();
    GPUDevice::Get().CreateSwapchain(MainWindow::Get());
}

Engine::~Engine()
{
    /*
     * TODO: Automatically destroy all singletons in the order in which they
     * were created.
     */
}

void Engine::Run()
{
    Fatal("TODO");
}
