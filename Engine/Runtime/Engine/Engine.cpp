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

#include "Engine/Engine.h"

#include "Core/Filesystem.h"
#include "Core/String.h"
#include "Core/Thread.h"
#include "Core/Time.h"

#include "Engine/AssetManager.h"
#include "Engine/DebugManager.h"
#include "Engine/FrameAllocator.h"
#include "Engine/Game.h"
#include "Engine/ImGUI.h"
#include "Engine/Window.h"

#include "Entity/World.h"

#include "GPU/GPUDevice.h"

#include "Input/InputManager.h"

#include "Render/RenderManager.h"
#include "Render/ShaderManager.h"

#include <SDL.h>

#include <imgui.h>

SINGLETON_IMPL(Engine);
SINGLETON_IMPL(Game);

Engine::Engine() :
    mFrameIndex             (0),
    mFrameStartTime         (0),
    mLastFrameTime          (0),
    mDeltaTime              (0.0f),
    mFPSUpdateTime          (0),
    mFramesSinceFPSUpdate   (0),
    mFPS                    (0.0f)
{
    Thread::Init({});

    LogInfo("Hello, World!");

    /* Find the game class and get the engine configuration from it. */
    const MetaClass* gameClass = nullptr;
    MetaClass::Visit(
        [&] (const MetaClass& inMetaClass)
        {
            if (&inMetaClass != &Game::staticMetaClass &&
                Game::staticMetaClass.IsBaseOf(inMetaClass) &&
                inMetaClass.IsConstructable())
            {
                AssertMsg(!gameClass, "Multiple Game classes found");
                gameClass = &inMetaClass;
            }
        });

    if (!gameClass)
    {
        Fatal("Failed to find game class");
    }

    /* Objects are reference counted, but Singleton will not create a reference.
     * Manually add one here so that the instance stays alive. */
    ObjectPtr<Game> game = gameClass->Construct<Game>();
    game->Retain();

    InitSDL();

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

    new RenderManager();

    GPUDevice::Get().CreateSwapchain(MainWindow::Get());

    new ShaderManager();
    new InputManager();
    new ImGUIManager();
    new DebugManager();
    new AssetManager();

    Game::Get().Init();
}

Engine::~Engine()
{
    /*
     * TODO: Automatically destroy all singletons in the order in which they
     * were created.
     */
}

void Engine::InitSDL()
{
    /* This is for the window surface API which we don't use. Disabling
     * acceleration prevents SDL from unnecessarily loading libGL. */
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        Fatal("Failed to initialize SDL: %s", SDL_GetError());
    }
}

void Engine::Run()
{
    while (true)
    {
        const uint64_t newStartTime = Platform::GetPerformanceCounter();

        if (mFrameStartTime != 0)
        {
            mLastFrameTime = newStartTime - mFrameStartTime;
            mDeltaTime     = static_cast<double>(mLastFrameTime) / static_cast<double>(kNanosecondsPerSecond);

            mFramesSinceFPSUpdate++;

            const uint64_t timeSinceFPSUpdate = newStartTime - mFPSUpdateTime;
            if (timeSinceFPSUpdate >= kNanosecondsPerSecond)
            {
                mFPS = static_cast<double>(mFramesSinceFPSUpdate) / (static_cast<double>(timeSinceFPSUpdate) / static_cast<double>(kNanosecondsPerSecond));

                mFPSUpdateTime        = newStartTime;
                mFramesSinceFPSUpdate = 0;
            }
        }
        else
        {
            mFPSUpdateTime = newStartTime;
        }

        mFrameStartTime = newStartTime;
        mFrameIndex++;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // FIXME: Need an Engine::Quit() method.
            if ((event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE) || event.type == SDL_QUIT)
            {
                return;
            }

            if (InputManager::Get().HandleEvent(event, {}))
            {
                continue;
            }
        }

        ImGUIManager::Get().BeginFrame({});

        auto& debugManager = DebugManager::Get();
        debugManager.BeginFrame({});
        debugManager.AddText(StringUtils::Format("FPS: %.2f", mFPS));
        debugManager.AddText(StringUtils::Format("Frame time: %.2f ms", static_cast<double>(mLastFrameTime) / static_cast<double>(kNanosecondsPerMillisecond)));

        mWorld->Tick(mDeltaTime);

        RenderManager::Get().Render({});

        /* Render ImGUI as late as possible in the frame. What we render will
         * reflect everything submitted to ImGUI up until this point. */
        DebugManager::Get().RenderOverlay({});
        ImGUIManager::Get().Render({});

        /* Present the main window. This is done outside the render graph. */
        MainWindow::Get().Present({});

        GPUDevice::Get().EndFrame();

        /* This must be the very last call. */
        FrameAllocator::EndFrame({});
    }
}

void Engine::CreateWorld()
{
    mWorld.Reset();
    mWorld = new World;
}

void Engine::LoadWorld(const Path& inPath)
{
    mWorld.Reset();
    mWorld = AssetManager::Get().Load<World>(inPath);
}
