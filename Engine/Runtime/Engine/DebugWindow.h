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

#include "Core/String.h"

#include "Engine/ImGUI.h"

/** Base class for a window in the ImGUI debug overlay. */
class DebugWindow
{
public:
    /** Get the category of the window. */
    const std::string&          GetCategory() const { return mCategory; }

    /** Get/set the title of the window. */
    const std::string&          GetTitle() const    { return mTitle; }
    void                        SetTitle(std::string title)
                                    { mTitle = std::move(title); }

protected:
                                DebugWindow(std::string category,
                                            std::string title);
    virtual                     ~DebugWindow();

    /**
     * Render the contents of the window.
     *
     * This method will be called every frame when the overlay is active,
     * during DebugManager::RenderOverlay() near the end of the frame.
     *
     * Some window implementations may not be able to be drawn at that point
     * (e.g. ones which are based on transient state throughout a frame). In
     * this case, it is possible to leave this function empty and perform
     * rendering of the window manually at the appropriate point in the frame.
     * Begin()should be used to begin drawing the window, which will handle not
     * displaying it if the overlay or window are not visible.
     */
    virtual void                Render() {}

    bool                        Begin(const ImGuiWindowFlags flags = 0);

protected:
    const std::string           mCategory;
    std::string                 mTitle;

    bool                        mOpen;

    friend class DebugManager;
};
