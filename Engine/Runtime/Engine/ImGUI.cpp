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

#include "Engine/ImGUI.h"

#include "Engine/Engine.h"
#include "Engine/Window.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUSwapchain.h"

#include "Input/InputHandler.h"
#include "Input/InputManager.h"

#include "Render/RenderLayer.h"
#include "Render/RenderOutput.h"
#include "Render/ShaderManager.h"

SINGLETON_IMPL(ImGUIManager);

class ImGUIInputHandler final : public InputHandler
{
public:
                            ImGUIInputHandler();

protected:
    EventResult             HandleButton(const ButtonEvent& inEvent) override;
    EventResult             HandleAxis(const AxisEvent& inEvent) override;
    void                    HandleTextInput(const TextInputEvent& inEvent) override;

    friend class ImGUIManager;
};

class ImGUIRenderLayer final : public RenderLayer
{
public:
                            ImGUIRenderLayer();

public:
    void                    Render() override;

private:
    GPUShaderPtr            mVertexShader;
    GPUShaderPtr            mFragmentShader;
    GPUPipelinePtr          mPipeline;
    GPUTexturePtr           mFontTexture;
    GPUResourceViewPtr      mFontView;
    GPUSamplerRef           mSampler;

};

ImGUIManager::ImGUIManager() :
    mInputtingText  (false)
{
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    mInputHandler = new ImGUIInputHandler;
    mRenderLayer  = new ImGUIRenderLayer;
}

ImGUIManager::~ImGUIManager()
{
    delete mInputHandler;
    delete mRenderLayer;
}

void ImGUIManager::BeginFrame(OnlyCalledBy<Engine>)
{
    ImGuiIO& io = ImGui::GetIO();

    const glm::ivec2 size = MainWindow::Get().GetSize();
    io.DisplaySize        = ImVec2(size.x, size.y);
    io.DeltaTime          = Engine::Get().GetDeltaTime();

    /* Pass input state. */
    const InputModifier modifiers = InputManager::Get().GetModifiers();

    io.MousePos = InputManager::Get().GetCursorPosition();
    io.KeyShift = (modifiers & kInputModifier_Shift) != 0;
    io.KeyCtrl  = (modifiers & kInputModifier_Ctrl) != 0;
    io.KeyAlt   = (modifiers & kInputModifier_Alt) != 0;

    ImGui::NewFrame();

    if (io.WantTextInput != mInputtingText)
    {
        if (io.WantTextInput)
        {
            mInputHandler->BeginTextInput();
        }
        else
        {
            mInputHandler->EndTextInput();
        }

        mInputtingText = io.WantTextInput;
    }
}

ImGUIInputHandler::ImGUIInputHandler() :
    InputHandler    (kPriority_ImGUI)
{
    RegisterInputHandler();
}

InputHandler::EventResult ImGUIInputHandler::HandleButton(const ButtonEvent& inEvent)
{
    ImGuiIO& io = ImGui::GetIO();

    if (inEvent.code >= kInputCodeKeyboardFirst && inEvent.code <= kInputCodeKeyboardLast)
    {
        io.KeysDown[inEvent.code] = inEvent.down;
    }
    else if (inEvent.code >= kInputCodeMouseFirst && inEvent.code <= kInputCodeMouseLast)
    {
        switch (inEvent.code)
        {
            case kInputCode_MouseLeft:
                io.MouseDown[0] = inEvent.down;
                break;

            case kInputCode_MouseRight:
                io.MouseDown[1] = inEvent.down;
                break;

            case kInputCode_MouseMiddle:
                io.MouseDown[2] = inEvent.down;
                break;

            default:
                break;

        }
    }

    // FIXME
    return kEventResult_Stop;
}

InputHandler::EventResult ImGUIInputHandler::HandleAxis(const AxisEvent& inEvent)
{
    ImGuiIO& io = ImGui::GetIO();

    switch (inEvent.code)
    {
        case kInputCode_MouseScroll:
            io.MouseWheel = inEvent.delta;
            break;

        default:
            break;

    }

    // FIXME
    return kEventResult_Stop;
}

void ImGUIInputHandler::HandleTextInput(const TextInputEvent& inEvent)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharactersUTF8(inEvent.text.c_str());
}

ImGUIRenderLayer::ImGUIRenderLayer() :
    RenderLayer (RenderLayer::kOrder_ImGUI)
{
    SetLayerOutput(&MainWindow::Get());
    ActivateLayer();

    ImGuiIO& io = ImGui::GetIO();

    mVertexShader   = ShaderManager::Get().GetShader("Engine/ImGui.vert");
    mFragmentShader = ShaderManager::Get().GetShader("Engine/ImGui.frag");

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
    pipelineDesc.shaders[kGPUShaderStage_Vertex]   = mVertexShader;
    pipelineDesc.shaders[kGPUShaderStage_Fragment] = mFragmentShader;
    pipelineDesc.argumentSetLayouts[0]             = argumentLayout;
    pipelineDesc.blendState                        = GPUBlendState::Get(blendDesc);
    pipelineDesc.depthStencilState                 = GPUDepthStencilState::Get(GPUDepthStencilStateDesc());
    pipelineDesc.rasterizerState                   = GPURasterizerState::Get(rasterizerDesc);
    pipelineDesc.renderTargetState                 = GPURenderTargetState::Get(renderTargetDesc);
    pipelineDesc.vertexInputState                  = GPUVertexInputState::Get(vertexInputDesc);
    pipelineDesc.topology                          = kGPUPrimitiveTopology_TriangleList;

    mPipeline = GPUDevice::Get().CreatePipeline(pipelineDesc);

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

    mFontTexture = GPUDevice::Get().CreateTexture(textureDesc);

    GPUResourceViewDesc viewDesc;
    viewDesc.type     = kGPUResourceViewType_Texture2D;
    viewDesc.usage    = kGPUResourceUsage_ShaderRead;
    viewDesc.format   = textureDesc.format;
    viewDesc.mipCount = mFontTexture->GetNumMipLevels();

    mFontView = GPUDevice::Get().CreateResourceView(mFontTexture, viewDesc);

    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPUStagingTexture stagingTexture(GPUDevice::Get(), kGPUStagingAccess_Write, textureDesc);
    memcpy(stagingTexture.MapWrite({0, 0}), pixels, width * height * 4);
    stagingTexture.Finalise();

    graphicsContext.ResourceBarrier(mFontTexture, kGPUResourceState_None, kGPUResourceState_TransferWrite);
    graphicsContext.UploadTexture(mFontTexture, stagingTexture);
    graphicsContext.ResourceBarrier(mFontTexture, kGPUResourceState_TransferWrite, kGPUResourceState_AllShaderRead);

    GPUSamplerDesc samplerDesc;
    samplerDesc.minFilter = samplerDesc.magFilter = kGPUFilter_Linear;

    mSampler = GPUDevice::Get().GetSampler(samplerDesc);
}

void ImGUIRenderLayer::Render()
{
    ImGui::Render();

    const ImDrawData* const drawData = ImGui::GetDrawData();
    if (!drawData)
    {
        return;
    }

    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    GPURenderPass renderPass;
    renderPass.SetColour(0, GetLayerOutput()->GetRenderTargetView());

    GPUGraphicsCommandList* cmdList = graphicsContext.CreateRenderPass(renderPass);
    cmdList->Begin();
    cmdList->SetPipeline(mPipeline);

    GPUArgument arguments[3] = {};
    arguments[0].view    = mFontView;
    arguments[1].sampler = mSampler;

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