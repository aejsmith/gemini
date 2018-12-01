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

#include "Input/InputManager.h"

#include "Render/ShaderCompiler.h"

#include <SDL.h>

#include <imgui.h>

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

    new InputManager();

    Game::Get().Init();
}

Engine::~Engine()
{
    /*
     * TODO: Automatically destroy all singletons in the order in which they
     * were created.
     */
}

static GPUShaderPtr CreateShader(Path inPath, const GPUShaderStage inStage)
{
    GPUShaderCode code;
    bool isCompiled = ShaderCompiler::CompileFile(inPath, inStage, code);
    Assert(isCompiled);
    Unused(isCompiled);

    GPUShaderPtr shader = GPUDevice::Get().CreateShader(inStage, std::move(code));

    shader->SetName(inPath.GetString());

    return shader;
}

struct ImGuiResources
{
    GPUShaderPtr        vertexShader;
    GPUShaderPtr        fragmentShader;
    GPUPipelinePtr      pipeline;
    GPUTexturePtr       fontTexture;
    GPUResourceViewPtr  fontView;
    GPUSamplerRef       sampler;
};

static void InitImGui(ImGuiResources& outResources)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    outResources.vertexShader   = CreateShader("Engine/Shaders/ImGuiVert.glsl", kGPUShaderStage_Vertex);
    outResources.fragmentShader = CreateShader("Engine/Shaders/ImGuiFrag.glsl", kGPUShaderStage_Fragment);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(3);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Texture;
    argumentLayoutDesc.arguments[1] = kGPUArgumentType_Sampler;
    argumentLayoutDesc.arguments[2] = kGPUArgumentType_Uniforms;

    const GPUArgumentSetLayoutRef argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    GPUVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.buffers[0].stride = sizeof(ImDrawVert);
    vertexInputDesc.attributes[0].format = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[0].buffer = 0;
    vertexInputDesc.attributes[0].offset = offsetof(ImDrawVert, pos);
    vertexInputDesc.attributes[1].format = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[1].buffer = 0;
    vertexInputDesc.attributes[1].offset = offsetof(ImDrawVert, uv);
    vertexInputDesc.attributes[2].format = kGPUAttributeFormat_R8G8B8A8_UNorm;
    vertexInputDesc.attributes[2].buffer = 0;
    vertexInputDesc.attributes[2].offset = offsetof(ImDrawVert, col);

    GPURenderTargetStateDesc renderTargetDesc;
    renderTargetDesc.colour[0] = MainWindow::Get().GetSwapchain()->GetFormat();

    GPURasterizerStateDesc rasterizerDesc;
    rasterizerDesc.cullMode         = kGPUCullMode_None;
    rasterizerDesc.depthClampEnable = true;

    GPUBlendStateDesc blendDesc;
    blendDesc.attachments[0].enable          = true;
    blendDesc.attachments[0].srcColourFactor = kGPUBlendFactor_SrcAlpha;
    blendDesc.attachments[0].dstColourFactor = kGPUBlendFactor_OneMinusSrcAlpha;
    blendDesc.attachments[0].colourOp        = kGPUBlendOp_Add;
    blendDesc.attachments[0].srcAlphaFactor  = kGPUBlendFactor_OneMinusSrcAlpha;
    blendDesc.attachments[0].dstAlphaFactor  = kGPUBlendFactor_Zero;
    blendDesc.attachments[0].alphaOp         = kGPUBlendOp_Add;

    GPUPipelineDesc pipelineDesc;
    pipelineDesc.shaders[kGPUShaderStage_Vertex]   = outResources.vertexShader;
    pipelineDesc.shaders[kGPUShaderStage_Fragment] = outResources.fragmentShader;
    pipelineDesc.argumentSetLayouts[0]             = argumentLayout;
    pipelineDesc.blendState                        = GPUBlendState::Get(blendDesc);
    pipelineDesc.depthStencilState                 = GPUDepthStencilState::Get(GPUDepthStencilStateDesc());
    pipelineDesc.rasterizerState                   = GPURasterizerState::Get(rasterizerDesc);
    pipelineDesc.renderTargetState                 = GPURenderTargetState::Get(renderTargetDesc);
    pipelineDesc.vertexInputState                  = GPUVertexInputState::Get(vertexInputDesc);
    pipelineDesc.topology                          = kGPUPrimitiveTopology_TriangleList;

    outResources.pipeline = GPUDevice::Get().CreatePipeline(pipelineDesc);

    /* We use RGBA rather than just alpha here since the same shader supports
     * displaying custom textures. */
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    GPUTextureDesc textureDesc;
    textureDesc.type   = kGPUResourceType_Texture2D;
    textureDesc.usage  = kGPUResourceUsage_ShaderRead;
    textureDesc.format = PixelFormat::kR8G8B8A8;
    textureDesc.width  = width;
    textureDesc.height = height;

    outResources.fontTexture = GPUDevice::Get().CreateTexture(textureDesc);

    GPUResourceViewDesc viewDesc;
    viewDesc.type     = kGPUResourceViewType_Texture2D;
    viewDesc.usage    = kGPUResourceUsage_ShaderRead;
    viewDesc.format   = textureDesc.format;
    viewDesc.mipCount = outResources.fontTexture->GetNumMipLevels();

    outResources.fontView = GPUDevice::Get().CreateResourceView(outResources.fontTexture, viewDesc);

    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPUStagingTexture stagingTexture(GPUDevice::Get(), kGPUStagingAccess_Write, textureDesc);
    memcpy(stagingTexture.MapWrite({0, 0}), pixels, width * height * 4);
    stagingTexture.Finalise();

    graphicsContext.ResourceBarrier(outResources.fontTexture,
                                    kGPUResourceState_None,
                                    kGPUResourceState_TransferWrite);

    graphicsContext.UploadTexture(outResources.fontTexture, stagingTexture);

    graphicsContext.ResourceBarrier(outResources.fontTexture,
                                    kGPUResourceState_TransferWrite,
                                    kGPUResourceState_AllShaderRead);

    GPUSamplerDesc samplerDesc;
    samplerDesc.minFilter = samplerDesc.magFilter = kGPUFilter_Linear;

    outResources.sampler = GPUDevice::Get().GetSampler(samplerDesc);
}

static void BeginImGui()
{
    ImGuiIO& io = ImGui::GetIO();

    const glm::ivec2 size = MainWindow::Get().GetSize();
    io.DisplaySize = ImVec2(size.x, size.y);

    ImGui::NewFrame();
}

static void RenderImGui(const ImGuiResources& inResources)
{
    ImGui::Render();

    const ImDrawData* const drawData = ImGui::GetDrawData();
    if (!drawData)
    {
        return;
    }

    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPURenderPass renderPass;
    renderPass.SetColour(0, MainWindow::Get().GetSwapchain()->GetRenderTargetView());

    GPUGraphicsCommandList* cmdList = graphicsContext.CreateRenderPass(renderPass);
    cmdList->Begin();
    cmdList->SetPipeline(inResources.pipeline);

    GPUArgument arguments[3] = {};
    arguments[0].view    = inResources.fontView;
    arguments[1].sampler = inResources.sampler;

    cmdList->SetArguments(0, arguments);

    const glm::ivec2 winSize = MainWindow::Get().GetSize();

    const glm::mat4 projectionMatrix(
         2.0f / winSize.x, 0.0f,               0.0f, 0.0f,
         0.0f,             2.0f / -winSize.y,  0.0f, 0.0f,
         0.0f,             0.0f,              -1.0f, 0.0f,
        -1.0f,             1.0f,               0.0f, 1.0f);

    cmdList->WriteUniforms(0, 2, &projectionMatrix, sizeof(projectionMatrix));

    /* Keep created buffers alive until we submit the command list. */
    std::vector<GPUBufferPtr> buffers;

    GPUStagingBuffer stagingBuffer(GPUDevice::Get());

    for (int i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* const imCmdList = drawData->CmdLists[i];

        const uint32_t vertexDataSize = imCmdList->VtxBuffer.size() * sizeof(ImDrawVert);
        const uint32_t indexDataSize  = imCmdList->IdxBuffer.size() * sizeof(ImDrawIdx);
        const uint32_t bufferSize     = vertexDataSize + indexDataSize;

        stagingBuffer.Initialise(kGPUStagingAccess_Write, bufferSize);
        stagingBuffer.Write(&imCmdList->VtxBuffer.front(), vertexDataSize, 0);
        stagingBuffer.Write(&imCmdList->IdxBuffer.front(), indexDataSize, vertexDataSize);
        stagingBuffer.Finalise();

        GPUBufferDesc bufferDesc;
        bufferDesc.size = bufferSize;

        GPUBufferPtr buffer = GPUDevice::Get().CreateBuffer(bufferDesc);

        graphicsContext.UploadBuffer(buffer, stagingBuffer, bufferSize);
        graphicsContext.ResourceBarrier(buffer,
                                        kGPUResourceState_TransferWrite,
                                        kGPUResourceState_IndexBufferRead | kGPUResourceState_VertexBufferRead);

        cmdList->SetVertexBuffer(0, buffer, 0);
        cmdList->SetIndexBuffer(kGPUIndexType_16, buffer, vertexDataSize);

        buffers.emplace_back(std::move(buffer));

        size_t indexOffset = 0;

        for (const ImDrawCmd* cmd = imCmdList->CmdBuffer.begin(); cmd != imCmdList->CmdBuffer.end(); cmd++)
        {
            const IntRect scissor(cmd->ClipRect.x,
                                  cmd->ClipRect.y,
                                  cmd->ClipRect.z - cmd->ClipRect.x,
                                  cmd->ClipRect.w - cmd->ClipRect.y);

            cmdList->SetScissor(scissor);
            cmdList->DrawIndexed(cmd->ElemCount, indexOffset);

            indexOffset += cmd->ElemCount;
        }
    }

    cmdList->End();
    graphicsContext.SubmitRenderPass(cmdList);
}

void Engine::Run()
{
    ImGuiResources imGuiResources;
    InitImGui(imGuiResources);

    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPUSwapchain& swapchain = *MainWindow::Get().GetSwapchain();

    GPUShaderPtr vertexShader   = CreateShader("Engine/Shaders/TestVert.glsl", kGPUShaderStage_Vertex);
    GPUShaderPtr fragmentShader = CreateShader("Engine/Shaders/TestFrag.glsl", kGPUShaderStage_Fragment);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(1);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Uniforms;

    const GPUArgumentSetLayoutRef argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

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

    GPUVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.buffers[0].stride = sizeof(glm::vec2);
    vertexInputDesc.attributes[0].format = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[0].buffer = 0;
    vertexInputDesc.attributes[0].offset = 0;

    GPUVertexInputStateRef vertexInputState = GPUVertexInputState::Get(vertexInputDesc);

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

            if (InputManager::Get().HandleEvent(event, {}))
            {
                continue;
            }
        }

        BeginImGui();

        ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Test", nullptr, ImGuiWindowFlags_None))
        {
            ImGui::Text("Hello World!");
        }

        ImGui::End();

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
        pipelineDesc.vertexInputState                  = vertexInputState;
        pipelineDesc.topology                          = kGPUPrimitiveTopology_TriangleList;

        cmdList->SetPipeline(pipelineDesc);
        cmdList->SetVertexBuffer(0, vertexBuffer);
        cmdList->WriteUniforms(0, 0, kColours, sizeof(kColours));

        cmdList->Draw(3);

        cmdList->End();
        graphicsContext.SubmitRenderPass(cmdList);

        RenderImGui(imGuiResources);

        graphicsContext.ResourceBarrier(view, kGPUResourceState_RenderTarget, kGPUResourceState_Present);

        graphicsContext.EndPresent(swapchain);

        GPUDevice::Get().EndFrame();
    }
}
