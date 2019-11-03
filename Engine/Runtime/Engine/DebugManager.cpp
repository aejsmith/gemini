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

#include "Input/InputHandler.h"
#include "Input/InputManager.h"

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
}

DebugManager::~DebugManager()
{
    delete mInputHandler;
}

void DebugManager::BeginFrame(OnlyCalledBy<Engine>)
{
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
