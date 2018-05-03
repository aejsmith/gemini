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

#pragma once

#include "Core/Math.h"
#include "Core/Singleton.h"

#include <string>

struct SDL_Window;

class Window
{
public:
    enum Flags : uint32_t
    {
        kWindow_Fullscreen = (1 << 0),
        kWindow_Hidden     = (1 << 1),
    };

public:

                            Window(std::string       inTitle,
                                   const glm::ivec2& inSize,
                                   const uint32_t    inFlags);
    virtual                 ~Window();

public:
    SDL_Window*             GetSDLWindow() const    { return mSDLWindow; }

    const glm::ivec2&       GetSize() const         { return mSize; }
    bool                    IsFullscreen() const    { return mFlags & kWindow_Fullscreen; }
    bool                    IsHidden() const        { return mFlags & kWindow_Hidden; }

private:
    SDL_Window*             mSDLWindow;

    std::string             mTitle;
    glm::ivec2              mSize;
    uint32_t                mFlags;

};

class MainWindow : public Window,
                   public Singleton<MainWindow>
{
public:
                            MainWindow(const glm::ivec2& inSize,
                                       const uint32_t    inFlags);
};
