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

protected:
    bool                    mEnabled;

    friend class ImGUIManager;
};

class ImGUIRenderer
{
public:
                            ImGUIRenderer();
                            ~ImGUIRenderer();

    void                    Render() const;

private:
    GPUShaderPtr            mVertexShader;
    GPUShaderPtr            mPixelShader;
    GPUPipeline*            mPipeline;
    GPUTexture*             mFontTexture;
    GPUResourceView*        mFontView;
    GPUSamplerRef           mSampler;

};

ImGUIManager::ImGUIManager() :
    mInputtingText  (false)
{
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    /* Set up key mapping. */
    io.KeyMap[ImGuiKey_Tab]         = kInputCode_Tab;
    io.KeyMap[ImGuiKey_LeftArrow]   = kInputCode_Left;
    io.KeyMap[ImGuiKey_RightArrow]  = kInputCode_Right;
    io.KeyMap[ImGuiKey_UpArrow]     = kInputCode_Up;
    io.KeyMap[ImGuiKey_DownArrow]   = kInputCode_Down;
    io.KeyMap[ImGuiKey_PageUp]      = kInputCode_PageUp;
    io.KeyMap[ImGuiKey_PageDown]    = kInputCode_PageDown;
    io.KeyMap[ImGuiKey_Home]        = kInputCode_Home;
    io.KeyMap[ImGuiKey_End]         = kInputCode_End;
    io.KeyMap[ImGuiKey_Insert]      = kInputCode_Insert;
    io.KeyMap[ImGuiKey_Delete]      = kInputCode_Delete;
    io.KeyMap[ImGuiKey_Backspace]   = kInputCode_Backspace;
    io.KeyMap[ImGuiKey_Space]       = kInputCode_Space;
    io.KeyMap[ImGuiKey_Enter]       = kInputCode_Return;
    io.KeyMap[ImGuiKey_Escape]      = kInputCode_Escape;
    io.KeyMap[ImGuiKey_KeyPadEnter] = kInputCode_KPEnter;
    io.KeyMap[ImGuiKey_A]           = kInputCode_A;
    io.KeyMap[ImGuiKey_C]           = kInputCode_C;
    io.KeyMap[ImGuiKey_V]           = kInputCode_V;
    io.KeyMap[ImGuiKey_X]           = kInputCode_X;
    io.KeyMap[ImGuiKey_Y]           = kInputCode_Y;
    io.KeyMap[ImGuiKey_Z]           = kInputCode_Z;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 0.0f;

    mInputHandler = new ImGUIInputHandler;
    mRenderer     = new ImGUIRenderer;
}

ImGUIManager::~ImGUIManager()
{
    delete mInputHandler;
    delete mRenderer;
}

void ImGUIManager::BeginFrame(OnlyCalledBy<Engine>)
{
    ImGuiIO& io = ImGui::GetIO();

    const glm::ivec2 size = MainWindow::Get().GetSize();
    io.DisplaySize        = ImVec2(size.x, size.y);
    io.DeltaTime          = Engine::Get().GetDeltaTime();

    /* Pass input state. */
    const InputModifier modifiers = InputManager::Get().GetModifiers();

    io.MousePos = (mInputHandler->mEnabled) ? InputManager::Get().GetCursorPosition() : ImVec2(0, 0);
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

void ImGUIManager::Render(OnlyCalledBy<Engine>)
{
    mRenderer->Render();
}

ImGUIInputHandler::ImGUIInputHandler() :
    InputHandler    (kPriority_ImGUI),
    mEnabled        (false)
{
    RegisterInputHandler();
}

InputHandler::EventResult ImGUIInputHandler::HandleButton(const ButtonEvent& inEvent)
{
    if (!mEnabled)
    {
        return kEventResult_Continue;
    }

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

    return kEventResult_Continue;
}

InputHandler::EventResult ImGUIInputHandler::HandleAxis(const AxisEvent& inEvent)
{
    if (!mEnabled)
    {
        return kEventResult_Continue;
    }

    ImGuiIO& io = ImGui::GetIO();

    switch (inEvent.code)
    {
        case kInputCode_MouseScroll:
            io.MouseWheel = inEvent.delta;
            break;

        default:
            break;

    }

    return kEventResult_Continue;
}

void ImGUIInputHandler::HandleTextInput(const TextInputEvent& inEvent)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharactersUTF8(inEvent.text.c_str());
}

ImGUIRenderer::ImGUIRenderer()
{
    ImGuiIO& io = ImGui::GetIO();

    mVertexShader = ShaderManager::Get().GetShader("Engine/ImGui.hlsl", "VSMain", kGPUShaderStage_Vertex);
    mPixelShader  = ShaderManager::Get().GetShader("Engine/ImGui.hlsl", "PSMain", kGPUShaderStage_Pixel);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(3);
    argumentLayoutDesc.arguments[0] = kGPUArgumentType_Texture;
    argumentLayoutDesc.arguments[1] = kGPUArgumentType_Sampler;
    argumentLayoutDesc.arguments[2] = kGPUArgumentType_Constants;

    const GPUArgumentSetLayoutRef argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    GPUVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.buffers[0].stride = sizeof(ImDrawVert);
    vertexInputDesc.attributes[0].semantic = kGPUAttributeSemantic_Position;
    vertexInputDesc.attributes[0].format   = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[0].buffer   = 0;
    vertexInputDesc.attributes[0].offset   = offsetof(ImDrawVert, pos);
    vertexInputDesc.attributes[1].semantic = kGPUAttributeSemantic_TexCoord;
    vertexInputDesc.attributes[1].format   = kGPUAttributeFormat_R32G32_Float;
    vertexInputDesc.attributes[1].buffer   = 0;
    vertexInputDesc.attributes[1].offset   = offsetof(ImDrawVert, uv);
    vertexInputDesc.attributes[2].semantic = kGPUAttributeSemantic_Colour;
    vertexInputDesc.attributes[2].format   = kGPUAttributeFormat_R8G8B8A8_UNorm;
    vertexInputDesc.attributes[2].buffer   = 0;
    vertexInputDesc.attributes[2].offset   = offsetof(ImDrawVert, col);

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
    pipelineDesc.shaders[kGPUShaderStage_Vertex] = mVertexShader;
    pipelineDesc.shaders[kGPUShaderStage_Pixel]  = mPixelShader;
    pipelineDesc.argumentSetLayouts[0]           = argumentLayout;
    pipelineDesc.blendState                      = GPUBlendState::Get(blendDesc);
    pipelineDesc.depthStencilState               = GPUDepthStencilState::GetDefault();
    pipelineDesc.rasterizerState                 = GPURasterizerState::Get(rasterizerDesc);
    pipelineDesc.renderTargetState               = GPURenderTargetState::Get(renderTargetDesc);
    pipelineDesc.vertexInputState                = GPUVertexInputState::Get(vertexInputDesc);
    pipelineDesc.topology                        = kGPUPrimitiveTopology_TriangleList;

    mPipeline = GPUDevice::Get().GetPipeline(pipelineDesc);

    /* We use RGBA rather than just alpha here since the same shader supports
     * displaying custom textures. */
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    GPUTextureDesc textureDesc;
    textureDesc.type   = kGPUResourceType_Texture2D;
    textureDesc.usage  = kGPUResourceUsage_ShaderRead;
    textureDesc.format = kPixelFormat_R8G8B8A8;
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

    GPUStagingTexture stagingTexture(kGPUStagingAccess_Write, textureDesc);
    memcpy(stagingTexture.MapWrite({0, 0}), pixels, width * height * 4);
    stagingTexture.Finalise();

    graphicsContext.ResourceBarrier(mFontTexture, kGPUResourceState_None, kGPUResourceState_TransferWrite);
    graphicsContext.UploadTexture(mFontTexture, stagingTexture);
    graphicsContext.ResourceBarrier(mFontTexture, kGPUResourceState_TransferWrite, kGPUResourceState_AllShaderRead);

    GPUSamplerDesc samplerDesc;
    samplerDesc.minFilter = samplerDesc.magFilter = kGPUFilter_Linear;

    mSampler = GPUDevice::Get().GetSampler(samplerDesc);
}

ImGUIRenderer::~ImGUIRenderer()
{
    delete mFontView;
    delete mFontTexture;
}

void ImGUIRenderer::Render() const
{
    ImGui::Render();

    const ImDrawData* const drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0)
    {
        return;
    }

    GPUTexture* const texture = MainWindow::Get().GetTexture();

    GPUResourceViewDesc viewDesc;
    viewDesc.type     = kGPUResourceViewType_Texture2D;
    viewDesc.usage    = kGPUResourceUsage_RenderTarget;
    viewDesc.format   = texture->GetFormat();

    UPtr<GPUResourceView> view(GPUDevice::Get().CreateResourceView(texture, viewDesc));

    GPURenderPass renderPass;
    renderPass.SetColour(0, view.get());

    GPUGraphicsContext& context = GPUGraphicsContext::Get();
    SCOPED_DEBUG_MARKER(context, "ImGUI");

    context.ResourceBarrier(view.get(), kGPUResourceState_Present, kGPUResourceState_RenderTarget);

    GPUGraphicsCommandList* const cmdList = context.CreateRenderPass(renderPass);
    cmdList->Begin();

    cmdList->SetPipeline(mPipeline);

    GPUArgument arguments[3] = {};
    arguments[0].view    = mFontView;
    arguments[1].sampler = mSampler;

    cmdList->SetArguments(0, arguments);

    const int32_t width  = texture->GetWidth();
    const int32_t height = texture->GetHeight();

    const glm::mat4 projectionMatrix(
        2.0f / width, 0.0f,            0.0f, 0.0f,
        0.0f,         2.0f / -height,  0.0f, 0.0f,
        0.0f,         0.0f,           -1.0f, 0.0f,
        -1.0f,        1.0f,            0.0f, 1.0f);

    cmdList->WriteConstants(0, 2, &projectionMatrix, sizeof(projectionMatrix));

    for (int i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* const imDrawList = drawData->CmdLists[i];

        cmdList->WriteVertexBuffer(0,
                                   &imDrawList->VtxBuffer.front(),
                                   imDrawList->VtxBuffer.size() * sizeof(ImDrawVert));

        cmdList->WriteIndexBuffer(kGPUIndexType_16,
                                  &imDrawList->IdxBuffer.front(),
                                  imDrawList->IdxBuffer.size() * sizeof(ImDrawIdx));

        size_t indexOffset = 0;

        for (const ImDrawCmd* cmd = imDrawList->CmdBuffer.begin(); cmd != imDrawList->CmdBuffer.end(); cmd++)
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

    context.SubmitRenderPass(cmdList);

    context.ResourceBarrier(view.get(), kGPUResourceState_RenderTarget, kGPUResourceState_Present);
}

void ImGUIManager::SetInputEnabled(const bool inEnable)
{
    mInputHandler->mEnabled = inEnable;
}
