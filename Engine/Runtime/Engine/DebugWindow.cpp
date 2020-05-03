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

#include "Engine/DebugWindow.h"

#include "Engine/DebugManager.h"

DebugWindow::DebugWindow(std::string category,
                         std::string title) :
    mCategory   (std::move(category)),
    mTitle      (std::move(title)),
    mOpen       (false)
{
    Assert(!mCategory.empty());
    Assert(!mTitle.empty());

    DebugManager::Get().RegisterWindow(this, {});
}

DebugWindow::~DebugWindow()
{
    DebugManager::Get().UnregisterWindow(this, {});
}

bool DebugWindow::Begin(const ImGuiWindowFlags flags)
{
    if (!mOpen || !DebugManager::Get().IsOverlayVisible())
    {
        return false;
    }
    else if (!ImGui::Begin(GetTitle().c_str(), &mOpen, flags))
    {
        ImGui::End();
        return false;
    }

    return true;
}
