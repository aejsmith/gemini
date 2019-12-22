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

#include "Engine/DebugManager.h"

#include "Engine/DebugWindow.h"
#include "Engine/ImGUI.h"
#include "Engine/Window.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"

#include "Input/InputHandler.h"
#include "Input/InputManager.h"

#include "Render/RenderGraph.h"
#include "Render/RenderView.h"
#include "Render/ShaderManager.h"

#include "../../Shaders/DebugPrimitive.h"

SINGLETON_IMPL(DebugManager);

static constexpr char kDebugTextWindowName[] = "Debug Text";
static constexpr uint32_t kMenuBarHeight     = 19;

class DebugInputHandler final : public InputHandler
{
public:
                            DebugInputHandler(DebugManager* const inManager);

protected:
    EventResult             HandleButton(const ButtonEvent& inEvent) override;
    EventResult             HandleAxis(const AxisEvent& inEvent) override;

private:
    DebugManager*           mManager;
    bool                    mPreviousMouseCapture;

};

DebugInputHandler::DebugInputHandler(DebugManager* const inManager) :
    InputHandler            (kPriority_DebugOverlay),
    mManager                (inManager),
    mPreviousMouseCapture   (false)
{
    RegisterInputHandler();
}

InputHandler::EventResult DebugInputHandler::HandleButton(const ButtonEvent& inEvent)
{
    if (!inEvent.down)
    {
        auto SetState = [&] (const DebugManager::OverlayState inState)
        {
            if (mManager->mOverlayState < DebugManager::kOverlayState_Active &&
                inState >= DebugManager::kOverlayState_Active)
            {
                /* Set the global mouse capture state to false because we want
                 * to use the OS cursor. */
                mPreviousMouseCapture = InputManager::Get().IsMouseCaptured();
                InputManager::Get().SetMouseCaptured(false);
                ImGUIManager::Get().SetInputEnabled(true);
            }
            else if (mManager->mOverlayState >= DebugManager::kOverlayState_Active &&
                     inState < DebugManager::kOverlayState_Active)
            {
                InputManager::Get().SetMouseCaptured(mPreviousMouseCapture);
                ImGUIManager::Get().SetInputEnabled(false);
            }

            mManager->mOverlayState = inState;
        };

        if (inEvent.code == kInputCode_F1)
        {
            SetState((mManager->mOverlayState == DebugManager::kOverlayState_Inactive)
                         ? DebugManager::kOverlayState_Active
                         : DebugManager::kOverlayState_Inactive);
        }
        else if (inEvent.code == kInputCode_F2 &&
                 mManager->mOverlayState >= DebugManager::kOverlayState_Visible)
        {
            SetState((mManager->mOverlayState == DebugManager::kOverlayState_Visible)
                         ? DebugManager::kOverlayState_Active
                         : DebugManager::kOverlayState_Visible);
        }
    }

    /* While we're active, consume all input. We have a priority 1 below ImGUI,
     * so ImGUI will continue to receive input, everything else will be
     * blocked. */
    return (mManager->mOverlayState >= DebugManager::kOverlayState_Active)
               ? kEventResult_Stop
               : kEventResult_Continue;
}

InputHandler::EventResult DebugInputHandler::HandleAxis(const AxisEvent& inEvent)
{
    /* As above. */
    return (mManager->mOverlayState >= DebugManager::kOverlayState_Active)
               ? kEventResult_Stop
               : kEventResult_Continue;
}

DebugManager::DebugManager() :
    mInputHandler   (new DebugInputHandler(this)),
    mOverlayState   (kOverlayState_Inactive)
{
    mVertexShader = ShaderManager::Get().GetShader("Engine/DebugPrimitive.hlsl", "VSMain", kGPUShaderStage_Vertex);
    mPixelShader  = ShaderManager::Get().GetShader("Engine/DebugPrimitive.hlsl", "PSMain", kGPUShaderStage_Pixel);

    GPUArgumentSetLayoutDesc argumentLayoutDesc(2);
    static_assert(kArgumentSet_ViewEntity == 0 && kViewEntityArguments_ViewConstants == 0,
                  "View entity argument set indices not as expected");
    argumentLayoutDesc.arguments[kViewEntityArguments_ViewConstants]   = kGPUArgumentType_Constants;
    argumentLayoutDesc.arguments[kViewEntityArguments_EntityConstants] = kGPUArgumentType_Constants;

    mArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

    mBlendState        = GPUBlendState::Get(GPUBlendStateDesc());
    mDepthStencilState = GPUDepthStencilState::Get(GPUDepthStencilStateDesc());

    GPURasterizerStateDesc rasterizerDesc;
    rasterizerDesc.polygonMode = kGPUPolygonMode_Line;
    rasterizerDesc.cullMode    = kGPUCullMode_None;

    mRasterizerState = GPURasterizerState::Get(rasterizerDesc);

    GPUVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.buffers[0].stride = sizeof(glm::vec3);
    vertexInputDesc.attributes[0].semantic = kGPUAttributeSemantic_Position;
    vertexInputDesc.attributes[0].format   = kGPUAttributeFormat_R32G32B32_Float;
    vertexInputDesc.attributes[0].buffer   = 0;
    vertexInputDesc.attributes[0].offset   = 0;

    mVertexInputState = GPUVertexInputState::Get(vertexInputDesc);
}

DebugManager::~DebugManager()
{
    delete mInputHandler;
}

void DebugManager::BeginFrame(OnlyCalledBy<Engine>)
{
    /* Clear last frame's primitives. */
    {
        std::unique_lock lock(mPrimitivesLock);
        mPrimitives.clear();
    }

    /* Begin the debug text window. */
    const glm::ivec2 size = MainWindow::Get().GetSize();
    const uint32_t offset = (mOverlayState >= kOverlayState_Active) ? kMenuBarHeight : 0;
    ImGui::SetNextWindowSize(ImVec2(size.x - 20, size.y - 20 - offset));
    ImGui::SetNextWindowPos(ImVec2(10, 10 + offset));
    ImGui::Begin(kDebugTextWindowName,
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::End();
}

void DebugManager::RenderOverlay(OnlyCalledBy<Engine>)
{
    if (mOverlayState >= kOverlayState_Visible)
    {
        if (mOverlayState >= kOverlayState_Active)
        {
            /* Draw the main menu. */
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("Windows"))
                {
                    for (DebugWindow* const window : mWindows)
                    {
                        if (window->IsAvailable())
                        {
                            ImGui::MenuItem(window->GetTitle(), nullptr, &window->mOpen);
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }
        }

        for (DebugWindow* const window : mWindows)
        {
            if (window->IsAvailable() && window->mOpen)
            {
                window->Render();
            }
        }
    }
}

void DebugManager::AddText(const char* const inText,
                           const glm::vec4&  inColour)
{
    /* Creating a window with the same title appends to it. */
    ImGui::Begin(kDebugTextWindowName, nullptr, 0);
    ImGui::PushStyleColor(ImGuiCol_Text, inColour);
    ImGui::Text(inText);
    ImGui::PopStyleColor();
    ImGui::End();
}

void DebugManager::RegisterWindow(DebugWindow* const inWindow,
                                  OnlyCalledBy<DebugWindow>)
{
    mWindows.emplace_back(inWindow);
}

void DebugManager::UnregisterWindow(DebugWindow* const inWindow,
                                    OnlyCalledBy<DebugWindow>)
{
    mWindows.remove(inWindow);
}

void DebugManager::RenderPrimitives(const RenderView&          inView,
                                    RenderGraph&               inGraph,
                                    const RenderResourceHandle inTexture,
                                    RenderResourceHandle&      outNewTexture)
{
    {
        std::unique_lock lock(mPrimitivesLock);

        if (mPrimitives.empty())
        {
            /* There's nothing to render, just pass through the resource handle. */
            outNewTexture = inTexture;
            return;
        }
    }

    const GPUConstants viewConstants = inView.GetConstants();

    RenderGraphPass& pass = inGraph.AddPass("DebugPrimitives", kRenderGraphPassType_Render);

    pass.SetColour(0, inTexture, &outNewTexture);

    pass.SetFunction([this, viewConstants] (const RenderGraph&      inGraph,
                                            const RenderGraphPass&  inPass,
                                            GPUGraphicsCommandList& inCmdList)
    {
        std::unique_lock lock(mPrimitivesLock);

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]             = mVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]              = mPixelShader;
        pipelineDesc.argumentSetLayouts[kArgumentSet_ViewEntity] = mArgumentSetLayout;
        pipelineDesc.blendState                                  = mBlendState;
        pipelineDesc.depthStencilState                           = mDepthStencilState;
        pipelineDesc.rasterizerState                             = mRasterizerState;
        pipelineDesc.renderTargetState                           = inCmdList.GetRenderTargetState();
        pipelineDesc.vertexInputState                            = mVertexInputState;

        DebugPrimitiveConstants constants;
        constants.colour = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);

        std::vector<glm::vec3> vertices;
        std::vector<uint16_t> indices;

        for (const Primitive& primitive : mPrimitives)
        {
            vertices.clear();
            indices.clear();

            switch (primitive.type)
            {
                case kPrimitiveType_BoundingBox:
                {
                    pipelineDesc.topology = kGPUPrimitiveTopology_LineList;

                    const glm::vec3& minimum = primitive.boundingBox.GetMinimum();
                    const glm::vec3& maximum = primitive.boundingBox.GetMaximum();

                    /* Calculate corners (left/right, bottom/top, back/front). */
                    const glm::vec3 lbb(minimum.x, minimum.y, minimum.z);
                    const glm::vec3 lbf(minimum.x, minimum.y, maximum.z);
                    const glm::vec3 ltb(minimum.x, maximum.y, minimum.z);
                    const glm::vec3 ltf(minimum.x, maximum.y, maximum.z);
                    const glm::vec3 rbb(maximum.x, minimum.y, minimum.z);
                    const glm::vec3 rbf(maximum.x, minimum.y, maximum.z);
                    const glm::vec3 rtb(maximum.x, maximum.y, minimum.z);
                    const glm::vec3 rtf(maximum.x, maximum.y, maximum.z);

                    /* Draw a line for each side. */
                    vertices.emplace_back(lbb); vertices.emplace_back(rbb);
                    vertices.emplace_back(rbb); vertices.emplace_back(rbf);
                    vertices.emplace_back(rbf); vertices.emplace_back(lbf);
                    vertices.emplace_back(lbf); vertices.emplace_back(lbb);
                    vertices.emplace_back(ltb); vertices.emplace_back(rtb);
                    vertices.emplace_back(rtb); vertices.emplace_back(rtf);
                    vertices.emplace_back(rtf); vertices.emplace_back(ltf);
                    vertices.emplace_back(ltf); vertices.emplace_back(ltb);
                    vertices.emplace_back(lbb); vertices.emplace_back(ltb);
                    vertices.emplace_back(rbb); vertices.emplace_back(rtb);
                    vertices.emplace_back(rbf); vertices.emplace_back(rtf);
                    vertices.emplace_back(lbf); vertices.emplace_back(ltf);

                    break;
                }

                case kPrimitiveType_Line:
                {
                    pipelineDesc.topology = kGPUPrimitiveTopology_LineList;

                    vertices.emplace_back(primitive.line.GetStart());
                    vertices.emplace_back(primitive.line.GetEnd());

                    break;
                }

                case kPrimitiveType_Sphere:
                {
                    pipelineDesc.topology = kGPUPrimitiveTopology_TriangleList;

                    primitive.sphere.CreateGeometry(16, 16, vertices, indices);

                    break;
                }

                default:
                {
                    Unreachable();
                }
            }

            inCmdList.SetPipeline(pipelineDesc);

            inCmdList.SetConstants(kArgumentSet_ViewEntity,
                                   kViewEntityArguments_ViewConstants,
                                   viewConstants);

            if (constants.colour != primitive.colour)
            {
                constants.colour = primitive.colour;

                inCmdList.WriteConstants(kArgumentSet_ViewEntity,
                                         kViewEntityArguments_EntityConstants,
                                         &constants,
                                         sizeof(constants));
            }

            inCmdList.WriteVertexBuffer(0, vertices.data(), vertices.size() * sizeof(vertices[0]));

            if (!indices.empty())
            {
                inCmdList.WriteIndexBuffer(kGPUIndexType_16, indices.data(), indices.size() * sizeof(indices[0]));

                inCmdList.DrawIndexed(static_cast<uint16_t>(indices.size()));
            }
            else
            {
                inCmdList.Draw(static_cast<uint32_t>(vertices.size()));
            }
        }
    });
}

DebugManager::Primitive::Primitive(const Primitive& inOther)
{
    /* All members have non-trivial constructors hence we need to define this
     * due to the union (copy constructor is otherwise implicitly deleted),
     * however it is safe to just memcpy(). */
    memcpy(this, &inOther, sizeof(*this));
}

void DebugManager::DrawPrimitive(const BoundingBox& inBox,
                                 const glm::vec3&   inColour)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type        = kPrimitiveType_BoundingBox;
    primitive.boundingBox = inBox;
    primitive.colour      = inColour;
}

void DebugManager::DrawPrimitive(const Line&        inLine,
                                 const glm::vec3&   inColour)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type   = kPrimitiveType_Line;
    primitive.line   = inLine;
    primitive.colour = inColour;
}

void DebugManager::DrawPrimitive(const Sphere&      inSphere,
                                 const glm::vec3&   inColour)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type   = kPrimitiveType_Sphere;
    primitive.sphere = inSphere;
    primitive.colour = inColour;
}
