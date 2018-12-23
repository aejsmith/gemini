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

#include "Engine/Window.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUSwapchain.h"

#include "Render/RenderLayer.h"
#include "Render/ShaderManager.h"

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
    mVertexShader   = ShaderManager::Get().GetShader("Game/Test.vert");
    mFragmentShader = ShaderManager::Get().GetShader("Game/Test.frag");

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
    mRenderLayer->ActivateLayer();
}

const char* TestGame::GetName() const
{
    return "Test";
}
