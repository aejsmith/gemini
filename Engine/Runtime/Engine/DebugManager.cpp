/*
 * Copyright (C) 2018-2020 Alex Smith
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
#include "Render/RenderManager.h"
#include "Render/RenderView.h"
#include "Render/ShaderManager.h"

#include "../../Shaders/DebugPrimitive.h"

#include "imgui_internal.h"

SINGLETON_IMPL(DebugManager);

static constexpr char kDebugTextWindowName[] = "Debug Text";

class DebugInputHandler final : public InputHandler
{
public:
                            DebugInputHandler(DebugManager* const manager);

protected:
    EventResult             HandleButton(const ButtonEvent& event) override;
    EventResult             HandleAxis(const AxisEvent& event) override;

private:
    DebugManager*           mManager;
    bool                    mPreviousMouseCapture;

};

DebugInputHandler::DebugInputHandler(DebugManager* const manager) :
    InputHandler            (kPriority_DebugOverlay),
    mManager                (manager),
    mPreviousMouseCapture   (false)
{
    RegisterInputHandler();
}

InputHandler::EventResult DebugInputHandler::HandleButton(const ButtonEvent& event)
{
    if (!event.down)
    {
        auto SetState = [&] (const DebugManager::OverlayState state)
        {
            if (mManager->mOverlayState < DebugManager::kOverlayState_Active &&
                state >= DebugManager::kOverlayState_Active)
            {
                /* Set the global mouse capture state to false because we want
                 * to use the OS cursor. */
                mPreviousMouseCapture = InputManager::Get().IsMouseCaptured();
                InputManager::Get().SetMouseCaptured(false);
                ImGUIManager::Get().SetInputEnabled(true);
            }
            else if (mManager->mOverlayState >= DebugManager::kOverlayState_Active &&
                     state < DebugManager::kOverlayState_Active)
            {
                InputManager::Get().SetMouseCaptured(mPreviousMouseCapture);
                ImGUIManager::Get().SetInputEnabled(false);
            }

            mManager->mOverlayState = state;
        };

        if (event.code == kInputCode_F1)
        {
            SetState((mManager->mOverlayState == DebugManager::kOverlayState_Inactive)
                         ? DebugManager::kOverlayState_Active
                         : DebugManager::kOverlayState_Inactive);
        }
        else if (event.code == kInputCode_F2 &&
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

InputHandler::EventResult DebugInputHandler::HandleAxis(const AxisEvent& event)
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

    /* Begin the docking space. */
    ImGuiID dockSpaceID = 0;
    if (mOverlayState >= kOverlayState_Visible)
    {
        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        if (mOverlayState >= kOverlayState_Active)
        {
            windowFlags |= ImGuiWindowFlags_MenuBar;
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("Overlay", nullptr, windowFlags);

        ImGui::PopStyleVar(2);

        dockSpaceID = ImGui::GetID("OverlayDockSpace");
        ImGui::DockSpace(dockSpaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        if (mOverlayState >= kOverlayState_Active && ImGui::BeginMenuBar())
        {
            for (const auto& it : mOverlayCategories)
            {
                const bool empty = it.second.windows.empty() && !it.second.menuFunction;

                if (!empty && ImGui::BeginMenu(it.first.c_str()))
                {
                    if (it.second.menuFunction)
                    {
                        it.second.menuFunction();
                        ImGui::Separator();
                    }

                    for (DebugWindow* const window : it.second.windows)
                    {
                        ImGui::MenuItem(window->GetTitle().c_str(), nullptr, &window->mOpen);
                    }

                    ImGui::EndMenu();
                }
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    /* Begin the debug text window. This lives in the remaining space that
     * nothing is docked in. */
    glm::uvec2 textRegionPos;
    glm::uvec2 textRegionSize;
    if (mOverlayState >= kOverlayState_Visible)
    {
        const ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dockSpaceID);
        textRegionPos  = glm::vec2(node->Pos);
        textRegionSize = glm::vec2(node->Size);
    }
    else
    {
        textRegionPos  = glm::uvec2(0, 0);
        textRegionSize = MainWindow::Get().GetSize();
    }

    ImGui::SetNextWindowSize(ImVec2(textRegionSize.x - 20, textRegionSize.y - 20));
    ImGui::SetNextWindowPos(ImVec2(textRegionPos.x + 10, textRegionPos.y + 10));
    ImGui::Begin(kDebugTextWindowName,
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::End();
}

void DebugManager::RenderOverlay(OnlyCalledBy<Engine>)
{
    if (mOverlayState >= kOverlayState_Visible)
    {
        for (const auto& it : mOverlayCategories)
        {
            for (DebugWindow* const window : it.second.windows)
            {
                if (window->mOpen)
                {
                    window->Render();
                }
            }
        }
    }
}

void DebugManager::AddText(const char* const text,
                           const glm::vec4&  colour)
{
    /* Creating a window with the same title appends to it. */
    ImGui::Begin(kDebugTextWindowName, nullptr, 0);
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::Text(text);
    ImGui::PopStyleColor();
    ImGui::End();
}

void DebugManager::RegisterWindow(DebugWindow* const window,
                                  OnlyCalledBy<DebugWindow>)
{
    OverlayCategory& category = mOverlayCategories[window->GetCategory()];
    category.windows.emplace_back(window);
}

void DebugManager::UnregisterWindow(DebugWindow* const window,
                                    OnlyCalledBy<DebugWindow>)
{
    OverlayCategory& category = mOverlayCategories[window->GetCategory()];
    category.windows.remove(window);
}

void DebugManager::AddOverlayMenu(const char* const     name,
                                  std::function<void()> function)
{
    OverlayCategory& category = mOverlayCategories[name];
    category.menuFunction = std::move(function);
}

void DebugManager::RenderPrimitives(const RenderView&     view,
                                    RenderGraph&          graph,
                                    RenderResourceHandle& ioDestTexture)
{
    {
        std::unique_lock lock(mPrimitivesLock);

        if (mPrimitives.empty())
        {
            return;
        }
    }

    const GPUConstants viewConstants = view.GetConstants();

    RenderGraphPass& pass = graph.AddPass("DebugPrimitives", kRenderGraphPassType_Render);

    pass.SetColour(0, ioDestTexture, &ioDestTexture);

    pass.SetFunction([this, viewConstants] (const RenderGraph&      graph,
                                            const RenderGraphPass&  pass,
                                            GPUGraphicsCommandList& cmdList)
    {
        std::unique_lock lock(mPrimitivesLock);

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]             = mVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]              = mPixelShader;
        pipelineDesc.argumentSetLayouts[kArgumentSet_ViewEntity] = RenderManager::Get().GetViewEntityArgumentSetLayout();
        pipelineDesc.depthStencilState                           = GPUDepthStencilState::GetDefault();
        pipelineDesc.renderTargetState                           = cmdList.GetRenderTargetState();
        pipelineDesc.vertexInputState                            = mVertexInputState;

        DebugPrimitiveConstants constants;
        constants.colour = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);

        std::vector<glm::vec3> vertices;
        std::vector<uint16_t> indices;

        for (const Primitive& primitive : mPrimitives)
        {
            vertices.clear();
            indices.clear();

            switch (primitive.type)
            {
                case kPrimitiveType_BoundingBox:
                case kPrimitiveType_Frustum:
                {
                    /* Calculate corners (left/right, bottom/top, back/front). */
                    glm::vec3 lbb, lbf, ltb, ltf, rbb, rbf, rtb, rtf;

                    if (primitive.type == kPrimitiveType_BoundingBox)
                    {
                        const glm::vec3& minimum = primitive.boundingBox.GetMinimum();
                        const glm::vec3& maximum = primitive.boundingBox.GetMaximum();

                        lbb = glm::vec3(minimum.x, minimum.y, minimum.z);
                        lbf = glm::vec3(minimum.x, minimum.y, maximum.z);
                        ltb = glm::vec3(minimum.x, maximum.y, minimum.z);
                        ltf = glm::vec3(minimum.x, maximum.y, maximum.z);
                        rbb = glm::vec3(maximum.x, minimum.y, minimum.z);
                        rbf = glm::vec3(maximum.x, minimum.y, maximum.z);
                        rtb = glm::vec3(maximum.x, maximum.y, minimum.z);
                        rtf = glm::vec3(maximum.x, maximum.y, maximum.z);
                    }
                    else
                    {
                        lbb = primitive.frustum.GetCorner(Frustum::kCorner_FarBottomLeft);
                        lbf = primitive.frustum.GetCorner(Frustum::kCorner_NearBottomLeft);
                        ltb = primitive.frustum.GetCorner(Frustum::kCorner_FarTopLeft);
                        ltf = primitive.frustum.GetCorner(Frustum::kCorner_NearTopLeft);
                        rbb = primitive.frustum.GetCorner(Frustum::kCorner_FarBottomRight);
                        rbf = primitive.frustum.GetCorner(Frustum::kCorner_NearBottomRight);
                        rtb = primitive.frustum.GetCorner(Frustum::kCorner_FarTopRight);
                        rtf = primitive.frustum.GetCorner(Frustum::kCorner_NearTopRight);
                    }

                    if (primitive.fill)
                    {
                        /* Draw 2 triangles for each face. */
                        pipelineDesc.topology = kGPUPrimitiveTopology_TriangleList;

                        vertices.emplace_back(lbb); vertices.emplace_back(ltb); vertices.emplace_back(rbb);
                        vertices.emplace_back(rbb); vertices.emplace_back(ltb); vertices.emplace_back(rtb);

                        vertices.emplace_back(rbf); vertices.emplace_back(rtf); vertices.emplace_back(lbf);
                        vertices.emplace_back(lbf); vertices.emplace_back(rtf); vertices.emplace_back(ltf);

                        vertices.emplace_back(lbf); vertices.emplace_back(ltf); vertices.emplace_back(lbb);
                        vertices.emplace_back(lbb); vertices.emplace_back(ltf); vertices.emplace_back(ltb);

                        vertices.emplace_back(rbb); vertices.emplace_back(rtb); vertices.emplace_back(rbf);
                        vertices.emplace_back(rbf); vertices.emplace_back(rtb); vertices.emplace_back(rtf);

                        vertices.emplace_back(rtf); vertices.emplace_back(rtb); vertices.emplace_back(ltf);
                        vertices.emplace_back(ltf); vertices.emplace_back(rtb); vertices.emplace_back(ltb);

                        vertices.emplace_back(rbb); vertices.emplace_back(rbf); vertices.emplace_back(lbb);
                        vertices.emplace_back(lbb); vertices.emplace_back(rbf); vertices.emplace_back(lbf);
                    }
                    else
                    {
                        /* Draw a line for each side. */
                        pipelineDesc.topology = kGPUPrimitiveTopology_LineList;

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
                    }

                    break;
                }

                case kPrimitiveType_Cone:
                {
                    pipelineDesc.topology = kGPUPrimitiveTopology_TriangleList;

                    primitive.cone.CreateGeometry(16, vertices, indices);

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

            GPURasterizerStateDesc rasterizerDesc;
            rasterizerDesc.polygonMode = (primitive.fill) ? kGPUPolygonMode_Fill : kGPUPolygonMode_Line;
            rasterizerDesc.cullMode    = (primitive.fill) ? kGPUCullMode_Back : kGPUCullMode_None;

            pipelineDesc.rasterizerState = GPURasterizerState::Get(rasterizerDesc);

            GPUBlendStateDesc blendDesc;

            if (primitive.colour.a != 1.0f)
            {
                blendDesc.attachments[0].enable          = true;
                blendDesc.attachments[0].srcColourFactor = kGPUBlendFactor_SrcAlpha;
                blendDesc.attachments[0].dstColourFactor = kGPUBlendFactor_OneMinusSrcAlpha;
                blendDesc.attachments[0].srcAlphaFactor  = kGPUBlendFactor_SrcAlpha;
                blendDesc.attachments[0].dstAlphaFactor  = kGPUBlendFactor_OneMinusSrcAlpha;
            }

            pipelineDesc.blendState = GPUBlendState::Get(blendDesc);

            cmdList.SetPipeline(pipelineDesc);

            cmdList.SetConstants(kArgumentSet_ViewEntity,
                                 kViewEntityArguments_ViewConstants,
                                 viewConstants);

            if (constants.colour != primitive.colour)
            {
                constants.colour = primitive.colour;

                cmdList.WriteConstants(kArgumentSet_ViewEntity,
                                       kViewEntityArguments_EntityConstants,
                                       constants);
            }

            cmdList.WriteVertexBuffer(0, vertices.data(), vertices.size() * sizeof(vertices[0]));

            if (!indices.empty())
            {
                cmdList.WriteIndexBuffer(kGPUIndexType_16, indices.data(), indices.size() * sizeof(indices[0]));

                cmdList.DrawIndexed(static_cast<uint16_t>(indices.size()));
            }
            else
            {
                cmdList.Draw(static_cast<uint32_t>(vertices.size()));
            }
        }
    });
}

DebugManager::Primitive::Primitive(const Primitive& other)
{
    /* All members have non-trivial constructors hence we need to define this
     * due to the union (copy constructor is otherwise implicitly deleted),
     * however it is safe to just memcpy(). */
    memcpy(this, &other, sizeof(*this));
}

void DebugManager::DrawPrimitive(const BoundingBox& box,
                                 const glm::vec4&   colour,
                                 const bool         fill)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type        = kPrimitiveType_BoundingBox;
    primitive.boundingBox = box;
    primitive.colour      = colour;
    primitive.fill        = fill;
}

void DebugManager::DrawPrimitive(const Frustum&     frustum,
                                 const glm::vec4&   colour,
                                 const bool         fill)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type    = kPrimitiveType_Frustum;
    primitive.frustum = frustum;
    primitive.colour  = colour;
    primitive.fill    = fill;
}

void DebugManager::DrawPrimitive(const Cone&      cone,
                                 const glm::vec4& colour,
                                 const bool       fill)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type   = kPrimitiveType_Cone;
    primitive.cone   = cone;
    primitive.colour = colour;
    primitive.fill   = fill;
}

void DebugManager::DrawPrimitive(const Line&      line,
                                 const glm::vec4& colour)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type   = kPrimitiveType_Line;
    primitive.line   = line;
    primitive.colour = colour;
    primitive.fill   = false;
}

void DebugManager::DrawPrimitive(const Sphere&    sphere,
                                 const glm::vec4& colour,
                                 const bool       fill)
{
    std::unique_lock lock(mPrimitivesLock);

    Primitive& primitive = mPrimitives.emplace_back();
    primitive.type   = kPrimitiveType_Sphere;
    primitive.sphere = sphere;
    primitive.colour = colour;
    primitive.fill   = fill;
}
