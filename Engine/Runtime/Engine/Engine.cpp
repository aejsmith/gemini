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

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUStagingResource.h"
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
    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPUSwapchain& swapchain = *MainWindow::Get().GetSwapchain();

    auto CreateShader =
        [] (Path inPath, const GPUShaderStage inStage) -> GPUShaderPtr
        {
            GPUShaderCode code;
            bool isCompiled = ShaderCompiler::CompileFile(inPath, inStage, code);
            Assert(isCompiled);
            Unused(isCompiled);

            return GPUDevice::Get().CreateShader(inStage, std::move(code));
        };

    GPUShaderPtr vertexShader   = CreateShader("Engine/Shaders/TestVert.glsl", kGPUShaderStage_Vertex);
    GPUShaderPtr fragmentShader = CreateShader("Engine/Shaders/TestFrag.glsl", kGPUShaderStage_Fragment);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(2);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Buffer;
    argumentLayoutDesc.arguments[1] = kGPUArgumentType_Uniforms;

    GPUArgumentSetLayout* argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    static const glm::vec2 kVertices[3] =
    {
        glm::vec2(-0.3f, -0.4f),
        glm::vec2( 0.3f, -0.4f),
        glm::vec2( 0.0f,  0.4f)
    };

    static const glm::vec4 kColours[3] =
    {
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    GPUBufferDesc vertexBufferDesc;
    vertexBufferDesc.usage = kGPUResourceUsage_ShaderRead;
    vertexBufferDesc.size  = sizeof(kVertices);

    GPUBufferPtr vertexBuffer = GPUDevice::Get().CreateBuffer(vertexBufferDesc);

    GPUResourceViewDesc vertexViewDesc;
    vertexViewDesc.type         = kGPUResourceViewType_Buffer;
    vertexViewDesc.usage        = kGPUResourceUsage_ShaderRead;
    vertexViewDesc.elementCount = sizeof(kVertices);

    GPUResourceViewPtr vertexView = GPUDevice::Get().CreateResourceView(vertexBuffer, vertexViewDesc);

    GPUArgument arguments[2] = {};
    arguments[0].view = vertexView;

    GPUArgumentSetPtr argumentSet = GPUDevice::Get().CreateArgumentSet(argumentLayout, arguments);

    {
        GPUStagingBuffer stagingBuffer(GPUDevice::Get(), kGPUStagingAccess_Write, sizeof(kVertices));
        stagingBuffer.Write(kVertices, sizeof(kVertices));
        stagingBuffer.Finalise();

        graphicsContext.UploadBuffer(vertexBuffer, stagingBuffer, sizeof(kVertices));

        graphicsContext.ResourceBarrier(vertexBuffer, kGPUResourceState_TransferWrite, kGPUResourceState_AllShaderRead);
    }

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

        graphicsContext.BeginPresent(swapchain);

        GPUResourceView* const view = swapchain.GetRenderTargetView();

        graphicsContext.ResourceBarrier(view, kGPUResourceState_Present, kGPUResourceState_RenderTarget);

        GPURenderPass renderPass;
        renderPass.SetColour(0, view);
        renderPass.ClearColour(0, glm::vec4(0.0f, 0.2f, 0.4f, 1.0f));

        GPUGraphicsCommandList* cmdList = graphicsContext.CreateRenderPass(renderPass);
        cmdList->Begin();

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]   = vertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Fragment] = fragmentShader;
        pipelineDesc.argumentSetLayouts[0]             = argumentLayout;
        pipelineDesc.blendState                        = GPUBlendState::Get(GPUBlendStateDesc());
        pipelineDesc.depthStencilState                 = GPUDepthStencilState::Get(GPUDepthStencilStateDesc());
        pipelineDesc.rasterizerState                   = GPURasterizerState::Get(GPURasterizerStateDesc());
        pipelineDesc.renderTargetState                 = cmdList->GetRenderTargetState();
        pipelineDesc.topology                          = kGPUPrimitiveTopology_TriangleList;

        cmdList->SetPipeline(pipelineDesc);

        cmdList->SetArguments(0, argumentSet);
        cmdList->WriteUniforms(0, 1, kColours, sizeof(kColours));

        cmdList->Draw(3);

        cmdList->End();
        graphicsContext.SubmitRenderPass(cmdList);

        graphicsContext.ResourceBarrier(view, kGPUResourceState_RenderTarget, kGPUResourceState_Present);

#if 0
        GPUTexture* const texture   = swapchain.GetTexture();

        GPUTextureClearData clearData;
        clearData.type   = GPUTextureClearData::kColour;
        clearData.colour = glm::vec4(0.0f, 0.2f, 0.4f, 1.0f);

        graphicsContext.ResourceBarrier(texture, kGPUResourceState_Present, kGPUResourceState_TransferWrite);
        graphicsContext.ClearTexture(texture, clearData);
        graphicsContext.ResourceBarrier(texture, kGPUResourceState_TransferWrite, kGPUResourceState_Present);
#endif

        graphicsContext.EndPresent(swapchain);

        GPUDevice::Get().EndFrame();
    }
}
