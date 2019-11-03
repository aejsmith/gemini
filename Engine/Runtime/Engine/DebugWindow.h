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

#pragma once

#include "Core/CoreDefs.h"

#include "Engine/ImGUI.h"

/** Base class for a window in the ImGUI debug overlay. */
class DebugWindow
{
protected:
                                DebugWindow();
    virtual                     ~DebugWindow();

    /** Return whether the window is available to display. Defaults to true. */
    virtual bool                IsAvailable() const;

    /** Get the title of the window. */
    virtual const char*         GetTitle() const = 0;

    /** Render the contents of the window. */
    virtual void                Render() = 0;

    bool                        Begin(const ImGuiWindowFlags inFlags = 0);

protected:
    bool                        mOpen;

    friend class DebugManager;
};

inline bool DebugWindow::Begin(const ImGuiWindowFlags inFlags)
{
    if (!ImGui::Begin(GetTitle(), &mOpen, inFlags))
    {
        ImGui::End();
        return false;
    }

    return true;
}
