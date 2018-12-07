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
#include "Core/Time.h"

#include "Engine/DebugManager.h"
#include "Engine/Game.h"
#include "Engine/ImGUI.h"
#include "Engine/Window.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUSwapchain.h"

#include "Input/InputManager.h"

#include "Render/RenderLayer.h"
#include "Render/RenderManager.h"
#include "Render/ShaderCompiler.h"

#include <SDL.h>

#include <imgui.h>

SINGLETON_IMPL(Engine);
SINGLETON_IMPL(Game);

Engine::Engine() :
    mFrameStartTime         (0),
    mLastFrameTime          (0),
    mDeltaTime              (0.0f),
    mFPSUpdateTime          (0),
    mFramesSinceFPSUpdate   (0),
    mFPS                    (0.0f)
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

    new RenderManager();

    GPUDevice::Get().CreateSwapchain(MainWindow::Get());

    new InputManager();
    new ImGUIManager();
    new DebugManager();

    Game::Get().Init();
}

Engine::~Engine()
{
    /*
     * TODO: Automatically destroy all singletons in the order in which they
     * were created.
     */
}

class TestRenderLayer final : public RenderLayer
{
public:
                            TestRenderLayer();
                            ~TestRenderLayer() {}

public:
    void                    Render() override;

private:
    GPUShaderPtr            mVertexShader;
    GPUShaderPtr            mFragmentShader;
    GPUArgumentSetLayoutRef mArgumentLayout;
    GPUBufferPtr            mVertexBuffer;
    GPUVertexInputStateRef  mVertexInputState;
};

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

TestRenderLayer::TestRenderLayer() :
    RenderLayer (RenderLayer::kOrder_World)
{
    auto CreateShader =
        [] (Path inPath, const GPUShaderStage inStage)
        {
            GPUShaderCode code;
            bool isCompiled = ShaderCompiler::CompileFile(inPath, inStage, code);
            Assert(isCompiled);
            Unused(isCompiled);

            GPUShaderPtr shader = GPUDevice::Get().CreateShader(inStage, std::move(code));

            shader->SetName(inPath.GetString());

            return shader;
        };

    mVertexShader   = CreateShader("Engine/Shaders/TestVert.glsl", kGPUShaderStage_Vertex);
    mFragmentShader = CreateShader("Engine/Shaders/TestFrag.glsl", kGPUShaderStage_Fragment);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(1);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Uniforms;

    mArgumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    GPUBufferDesc vertexBufferDesc;
    vertexBufferDesc.usage = kGPUResourceUsage_ShaderRead;
    vertexBufferDesc.size  = sizeof(kVertices);

    mVertexBuffer = GPUDevice::Get().CreateBuffer(vertexBufferDesc);

    GPUVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.buffers[0].stride    = sizeof(glm::vec2);
    vertexInputDesc.attributes[0].format = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[0].buffer = 0;
    vertexInputDesc.attributes[0].offset = 0;

    mVertexInputState = GPUVertexInputState::Get(vertexInputDesc);

    GPUStagingBuffer stagingBuffer(GPUDevice::Get(), kGPUStagingAccess_Write, sizeof(kVertices));
    stagingBuffer.Write(kVertices, sizeof(kVertices));
    stagingBuffer.Finalise();

    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    graphicsContext.UploadBuffer(mVertexBuffer, stagingBuffer, sizeof(kVertices));

    graphicsContext.ResourceBarrier(mVertexBuffer, kGPUResourceState_TransferWrite, kGPUResourceState_AllShaderRead);
}

void TestRenderLayer::Render()
{
    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPURenderPass renderPass;
    renderPass.SetColour(0, GetLayerOutput()->GetRenderTargetView());
    renderPass.ClearColour(0, glm::vec4(0.0f, 0.2f, 0.4f, 1.0f));

    GPUGraphicsCommandList* cmdList = graphicsContext.CreateRenderPass(renderPass);
    cmdList->Begin();

    GPUPipelineDesc pipelineDesc;
    pipelineDesc.shaders[kGPUShaderStage_Vertex]   = mVertexShader;
    pipelineDesc.shaders[kGPUShaderStage_Fragment] = mFragmentShader;
    pipelineDesc.argumentSetLayouts[0]             = mArgumentLayout;
    pipelineDesc.blendState                        = GPUBlendState::Get(GPUBlendStateDesc());
    pipelineDesc.depthStencilState                 = GPUDepthStencilState::Get(GPUDepthStencilStateDesc());
    pipelineDesc.rasterizerState                   = GPURasterizerState::Get(GPURasterizerStateDesc());
    pipelineDesc.renderTargetState                 = cmdList->GetRenderTargetState();
    pipelineDesc.vertexInputState                  = mVertexInputState;
    pipelineDesc.topology                          = kGPUPrimitiveTopology_TriangleList;

    cmdList->SetPipeline(pipelineDesc);
    cmdList->SetVertexBuffer(0, mVertexBuffer);
    cmdList->WriteUniforms(0, 0, kColours, sizeof(kColours));

    cmdList->Draw(3);

    cmdList->End();
    graphicsContext.SubmitRenderPass(cmdList);
}

void Engine::Run()
{
    TestRenderLayer testLayer;
    testLayer.SetLayerOutput(&MainWindow::Get());
    testLayer.ActivateLayer();

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
        DebugManager::Get().BeginFrame({});

        DebugManager::Get().AddText(StringUtils::Format("FPS: %.2f", mFPS));
        DebugManager::Get().AddText(StringUtils::Format("Frame time: %.2f ms", static_cast<double>(mLastFrameTime) / static_cast<double>(kNanosecondsPerMillisecond)));

        RenderManager::Get().Render({});

        GPUDevice::Get().EndFrame();
    }
}
