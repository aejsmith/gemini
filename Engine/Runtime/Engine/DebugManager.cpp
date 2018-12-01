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

#include "Engine/DebugManager.h"

#include "Engine/ImGUI.h"
#include "Engine/Window.h"

SINGLETON_IMPL(DebugManager);

static constexpr char kDebugTextWindowName[] = "Debug Text";

DebugManager::DebugManager()
{
}

void DebugManager::BeginFrame(OnlyCalledBy<Engine>)
{
    /* Begin the debug text window. */
    const glm::ivec2 size = MainWindow::Get().GetSize();
    ImGui::SetNextWindowSize(ImVec2(size.x - 20, size.y - 20));
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin(kDebugTextWindowName,
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::End();
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
