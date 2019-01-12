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

#include "TestGame.h"

#include "Core/Filesystem.h"

#include "Engine/Engine.h"
#include "Engine/Entity.h"
#include "Engine/JSONSerialiser.h"
#include "Engine/Window.h"
#include "Engine/World.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUSwapchain.h"

#include "Render/RenderLayer.h"
#include "Render/ShaderManager.h"

#include <thread>

static const uint32_t kNumColumns  = 100;
static const uint32_t kNumRows     = 50;
static const uint32_t kRepeat      = 10;
static const uint32_t kThreadCount = 4;

/* Leave spacing between them. */
static const uint32_t kTotalNumColumns = (kNumColumns * 2) + 1;
static const uint32_t kTotalNumRows    = (kNumRows * 2) + 1;

class TestRenderLayer final : public RenderLayer
{
public:
                                TestRenderLayer();
                                ~TestRenderLayer();

public:
    void                        Initialise();

    void                        Render() override;

private:
    struct Worker
    {
        std::thread             thread;
        std::atomic<bool>       ready       {false};
        uint32_t                rowOffset;
        uint32_t                rowCount;
        GPUGraphicsCommandList* cmdList;
    };

private:
    void                        WorkerThread(const uint32_t inIndex);

private:
    GPUTexture*                 mDepthBuffer;
    GPUResourceView*            mDepthView;
    GPUShaderPtr                mVertexShader;
    GPUShaderPtr                mPixelShader;
    GPUArgumentSetLayoutRef     mArgumentLayout;
    GPUBuffer*                  mVertexBuffer;
    GPUVertexInputStateRef      mVertexInputState;

    Worker                      mWorkers[kThreadCount];
    std::atomic<uint32_t>       mThreadsActive;
};

struct Vertex
{
    glm::vec2                   position;
    glm::vec4                   colour;
};

static const Vertex kVertices[3] =
{
    { glm::vec2(-1.0f, -1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
    { glm::vec2( 1.0f, -1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
    { glm::vec2( 0.0f,  1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
};

TestRenderLayer::TestRenderLayer() :
    RenderLayer (RenderLayer::kOrder_World)
{
}

TestRenderLayer::~TestRenderLayer()
{
    delete mVertexBuffer;
    delete mDepthView;
    delete mDepthBuffer;
}

void TestRenderLayer::Initialise()
{
    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    const auto& colourTexture = static_cast<GPUTexture&>(GetLayerOutput()->GetRenderTargetView()->GetResource());

    GPUTextureDesc depthBufferDesc;
    depthBufferDesc.type   = kGPUResourceType_Texture2D;
    depthBufferDesc.usage  = kGPUResourceUsage_DepthStencil;
    depthBufferDesc.format = PixelFormat::kDepth32;
    depthBufferDesc.width  = colourTexture.GetWidth();
    depthBufferDesc.height = colourTexture.GetHeight();

    mDepthBuffer = GPUDevice::Get().CreateTexture(depthBufferDesc);

    graphicsContext.ResourceBarrier(mDepthBuffer, kGPUResourceState_None, kGPUResourceState_DepthStencilWrite);

    GPUResourceViewDesc depthViewDesc;
    depthViewDesc.type   = kGPUResourceViewType_Texture2D;
    depthViewDesc.usage  = kGPUResourceUsage_DepthStencil;
    depthViewDesc.format = depthBufferDesc.format;

    mDepthView = GPUDevice::Get().CreateResourceView(mDepthBuffer, depthViewDesc);

    mVertexShader  = ShaderManager::Get().GetShader("Game/Test.hlsl", "VSMain", kGPUShaderStage_Vertex);
    mPixelShader   = ShaderManager::Get().GetShader("Game/Test.hlsl", "PSMain", kGPUShaderStage_Pixel);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(1);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Constants;

    mArgumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    GPUBufferDesc vertexBufferDesc;
    vertexBufferDesc.usage = kGPUResourceUsage_ShaderRead;
    vertexBufferDesc.size  = sizeof(kVertices);

    mVertexBuffer = GPUDevice::Get().CreateBuffer(vertexBufferDesc);

    GPUVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.buffers[0].stride    = sizeof(Vertex);
    vertexInputDesc.attributes[0].semantic = kGPUAttributeSemantic_Position;
    vertexInputDesc.attributes[0].format   = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[0].buffer   = 0;
    vertexInputDesc.attributes[0].offset   = offsetof(Vertex, position);
    vertexInputDesc.attributes[1].semantic = kGPUAttributeSemantic_Colour;
    vertexInputDesc.attributes[1].format   = kGPUAttributeFormat_R32G32B32A32_Float;
    vertexInputDesc.attributes[1].buffer   = 0;
    vertexInputDesc.attributes[1].offset   = offsetof(Vertex, colour);

    mVertexInputState = GPUVertexInputState::Get(vertexInputDesc);

    GPUStagingBuffer stagingBuffer(GPUDevice::Get(), kGPUStagingAccess_Write, sizeof(kVertices));
    stagingBuffer.Write(kVertices, sizeof(kVertices));
    stagingBuffer.Finalise();

    graphicsContext.UploadBuffer(mVertexBuffer, stagingBuffer, sizeof(kVertices));

    graphicsContext.ResourceBarrier(mVertexBuffer, kGPUResourceState_TransferWrite, kGPUResourceState_AllShaderRead);

    for (uint32_t i = 0; i < kThreadCount; i++)
    {
        mWorkers[i].thread = std::thread(&TestRenderLayer::WorkerThread, this, i);
    }
}

void TestRenderLayer::WorkerThread(const uint32_t inIndex)
{
    Worker& worker = mWorkers[inIndex];

    while (true)
    {
        while (!worker.ready.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        worker.ready.store(false, std::memory_order_release);

        GPUGraphicsCommandList* cmdList = worker.cmdList;
        cmdList->Begin();

        GPUDepthStencilStateDesc depthDesc;
        depthDesc.depthTestEnable  = true;
        depthDesc.depthWriteEnable = true;
        depthDesc.depthCompareOp   = kGPUCompareOp_Less;

        // TODO: Change to pre-created, measure perf.
        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex] = mVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]  = mPixelShader;
        pipelineDesc.argumentSetLayouts[0]           = mArgumentLayout;
        pipelineDesc.blendState                      = GPUBlendState::Get(GPUBlendStateDesc());
        pipelineDesc.depthStencilState               = GPUDepthStencilState::Get(depthDesc);
        pipelineDesc.rasterizerState                 = GPURasterizerState::Get(GPURasterizerStateDesc());
        pipelineDesc.renderTargetState               = cmdList->GetRenderTargetState();
        pipelineDesc.vertexInputState                = mVertexInputState;
        pipelineDesc.topology                        = kGPUPrimitiveTopology_TriangleList;

        cmdList->SetPipeline(pipelineDesc);
        cmdList->SetVertexBuffer(0, mVertexBuffer);

        const auto& texture       = static_cast<GPUTexture&>(GetLayerOutput()->GetRenderTargetView()->GetResource());
        const float textureWidth  = static_cast<float>(texture.GetWidth());
        const float textureHeight = static_cast<float>(texture.GetHeight());
        const float cellWidth     = 2.0f * ((textureWidth / static_cast<float>(kTotalNumColumns)) / textureWidth);
        const float cellHeight    = 2.0f * ((textureHeight / static_cast<float>(kTotalNumRows)) / textureHeight);

        Transform transform;
        transform.SetScale(glm::vec3(cellWidth / 2.0f, cellHeight / 2.0f, 1.0f));

        for (uint32_t y = worker.rowOffset; y < worker.rowOffset + worker.rowCount; y++)
        {
            if (!(y & 1)) continue;

            for (uint32_t x = 0; x < kTotalNumColumns; x++)
            {
                if (!(x & 1)) continue;

                transform.SetPosition(glm::vec3((cellWidth * (0.5f + x)) - 1.0f,
                                                (cellHeight * (0.5f + y)) - 1.0f,
                                                0.0f));

                cmdList->WriteConstants(0, 0, &transform.GetMatrix(), sizeof(transform.GetMatrix()));

                for (uint32_t i = 0; i < kRepeat; i++)
                {
                    cmdList->Draw(3);
                }
            }
        }

        cmdList->End();

        mThreadsActive.fetch_sub(1, std::memory_order_release);
    }
}

void TestRenderLayer::Render()
{
    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPURenderPass renderPass;
    renderPass.SetColour(0, GetLayerOutput()->GetRenderTargetView());
    renderPass.ClearColour(0, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderPass.SetDepthStencil(mDepthView);
    renderPass.ClearDepth(1.0);

    GPUGraphicsCommandList* cmdList = graphicsContext.CreateRenderPass(renderPass);
    cmdList->Begin();

    const uint32_t numRowsRounded = RoundUp(kTotalNumRows, kThreadCount);
    const uint32_t rowsPerThread  = numRowsRounded / kThreadCount;

    GPUCommandList* children[kThreadCount];

    mThreadsActive.store(kThreadCount);

    for (uint32_t i = 0; i < kThreadCount; i++)
    {
        Worker& worker = mWorkers[i];

        worker.rowOffset = i * rowsPerThread;
        worker.rowCount  = std::min(worker.rowOffset + rowsPerThread, kTotalNumRows) - worker.rowOffset;
        worker.cmdList   = cmdList->CreateChild();

        children[i] = worker.cmdList;

        worker.ready.store(true, std::memory_order_release);
    }

    while (mThreadsActive.load(std::memory_order_acquire) > 0)
    {
        std::this_thread::yield();
    }

    cmdList->SubmitChildren(children, kThreadCount);

    cmdList->End();
    graphicsContext.SubmitRenderPass(cmdList);
}

TestGame::TestGame()
{
}

TestGame::~TestGame()
{
    delete mRenderLayer;
}

void TestGame::Init()
{
    mRenderLayer = new TestRenderLayer;
    mRenderLayer->SetLayerOutput(&MainWindow::Get());
    mRenderLayer->Initialise();
    mRenderLayer->ActivateLayer();

    Engine::Get().LoadWorld("Game/Worlds/Test");

    //World* world = Engine::Get().GetWorld();

    //Entity* entity1 = world->CreateEntity("Test");
    //entity1->Translate(glm::vec3(0.0f, 1.5f, 0.0f));
    //entity1->SetActive(true);

    //JSONSerialiser serialiser;
    //std::vector<uint8_t> data = serialiser.Serialise(world);
    //std::unique_ptr<File> file(Filesystem::OpenFile("derp.object", kFileMode_Write | kFileMode_Create | kFileMode_Truncate));
    //file->Write(&data[0], data.size());
}

const char* TestGame::GetName() const
{
    return "Test";
}
