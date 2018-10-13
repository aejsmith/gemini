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

#include "Engine/Game.h"
#include "Engine/Window.h"

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUSwapchain.h"

#include "Render/ShaderCompiler.h"

#include <SDL.h>

SINGLETON_IMPL(Engine);
SINGLETON_IMPL(Game);

Engine::Engine()
{
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

    Game::Get().Init();
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
    /* Always present from graphics for now, but in future depending on
     * workload we may wish to present from compute. Probably ought to be
     * decided by the frame graph. */
    GPUGraphicsContext& presentContext = GPUGraphicsContext::Get();

    GPUSwapchain& swapchain = *MainWindow::Get().GetSwapchain();

    auto CreateShader =
        [] (Path inPath, const GPUShaderStage inStage) -> GPUShaderPtr
        {
            GPUShaderCode code;
            bool isCompiled = ShaderCompiler::CompileFile(inPath, inStage, code);

            Assert(isCompiled);

            return GPUDevice::Get().CreateShader(inStage, std::move(code));
        };

    GPUShaderPtr vertexShader   = CreateShader("Engine/Shaders/TestVert.glsl", kGPUShaderStage_Vertex);
    GPUShaderPtr fragmentShader = CreateShader("Engine/Shaders/TestFrag.glsl", kGPUShaderStage_Fragment);

    while (true)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // FIXME: Need an Engine::Quit() method.
            if ((event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE) || event.type == SDL_QUIT)
            {
                return;
            }
        }

        /* TODO: Do everything else! */

        presentContext.BeginPresent(swapchain);

        GPUResourceView* const view = swapchain.GetRenderTargetView();

        presentContext.ResourceBarrier(view, kGPUResourceState_Present, kGPUResourceState_RenderTarget);

        GPURenderPass renderPass;
        renderPass.SetColour(0, view);
        renderPass.ClearColour(0, glm::vec4(0.0f, 0.2f, 0.4f, 1.0f));

        GPUGraphicsCommandList* cmdList = presentContext.CreateRenderPass(renderPass);
        cmdList->Begin();

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]   = vertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Fragment] = fragmentShader;
        pipelineDesc.blendState                        = GPUBlendState::Get(GPUBlendStateDesc());
        pipelineDesc.depthStencilState                 = GPUDepthStencilState::Get(GPUDepthStencilStateDesc());
        pipelineDesc.rasterizerState                   = GPURasterizerState::Get(GPURasterizerStateDesc());
        pipelineDesc.renderTargetState                 = cmdList->GetRenderTargetState();
        pipelineDesc.topology                          = kGPUPrimitiveTopology_TriangleList;

        cmdList->SetPipeline(pipelineDesc);
        cmdList->Draw(3);

        cmdList->End();
        presentContext.SubmitRenderPass(cmdList);

        presentContext.ResourceBarrier(view, kGPUResourceState_RenderTarget, kGPUResourceState_Present);

#if 0
        GPUTexture* const texture   = swapchain.GetTexture();

        GPUTextureClearData clearData;
        clearData.type   = GPUTextureClearData::kColour;
        clearData.colour = glm::vec4(0.0f, 0.2f, 0.4f, 1.0f);

        presentContext.ResourceBarrier(texture, kGPUResourceState_Present, kGPUResourceState_TransferWrite);
        presentContext.ClearTexture(texture, clearData);
        presentContext.ResourceBarrier(texture, kGPUResourceState_TransferWrite, kGPUResourceState_Present);
#endif

        presentContext.EndPresent(swapchain);

        GPUDevice::Get().EndFrame();
    }
}
