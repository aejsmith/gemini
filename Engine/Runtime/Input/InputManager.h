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

#include "Core/Math.h"
#include "Core/Singleton.h"

#include "Input/InputDefs.h"

#include <list>

class Engine;
class InputHandler;

union SDL_Event;

class InputManager : public Singleton<InputManager>
{
public:
                                InputManager();
                                ~InputManager();

public:
    /** Get the current input modifier state. */
    InputModifier               GetModifiers();

    /** Get the state of a button input. */
    bool                        GetButtonState(const InputCode inCode);

    /** Get the current mouse cursor position, relative to the focused window. */
    glm::ivec2                  GetCursorPosition();

    /** Get/set whether the mouse is captured. */
    bool                        IsMouseCaptured() const { return mMouseCaptured; }
    void                        SetMouseCaptured(const bool inCaptured);

    /** Interface with InputHandler. */
    void                        RegisterHandler(InputHandler* const inHandler,
                                                OnlyCalledBy<InputHandler>);
    void                        UnregisterHandler(InputHandler* const inHandler,
                                                  OnlyCalledBy<InputHandler>);
    void                        BeginTextInput(InputHandler* const inHandler,
                                               OnlyCalledBy<InputHandler>);
    void                        EndTextInput(InputHandler* const inHandler,
                                             OnlyCalledBy<InputHandler>);

    bool                        HandleEvent(const SDL_Event& inEvent,
                                            OnlyCalledBy<Engine>);

private:
    bool                        mMouseCaptured;

    /** List of handlers, sorted by priority. */
    std::list<InputHandler*>    mHandlers;

    InputHandler*               mTextInputHandler;

};
