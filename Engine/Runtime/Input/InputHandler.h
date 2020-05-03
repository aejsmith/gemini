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

#pragma once

#include "Input/InputEvent.h"

/**
 * Classes which wish to handle input should derive from this class and
 * implement the handler methods. When requested, the handler will be added to
 * the input manager.
 */
class InputHandler
{
public:
    /**
     * Input handling priorities (highest to lowest). When an input is received
     * it will be passed to each registered handler in priority order, until
     * one indicates that the event shouldn't be passed down any further.
     */
    enum Priority
    {
        /** ImGUI. */
        kPriority_ImGUI         = 0,

        /** Debug overlay. */
        kPriority_DebugOverlay  = 1,

        /** Game UI. */
        kPriority_UI            = 10,

        /** Game world. */
        kPriority_World         = 20,
    };

    /**
     * Input handling result, used to determine whether to pass events down
     * to lower priority handlers.
     */
    enum EventResult
    {
        kEventResult_Continue,
        kEventResult_Stop,
    };

public:
    Priority                GetInputPriority() const    { return mPriority; }

protected:
                            explicit InputHandler(const Priority priority);
                            ~InputHandler();

protected:
    void                    SetInputPriority(const Priority priority);

    /** (Un)register with the input manager. */
    void                    RegisterInputHandler();
    void                    UnregisterInputHandler();

    /**
     * Begin/end text input. When text input is enabled, the input manager
     * starts collecting text input and delivers the input to this handler via
     * HandleTextInput().
     */
    void                    BeginTextInput();
    void                    EndTextInput();

    /**
     * Event handlers.
     */

    virtual EventResult     HandleButton(const ButtonEvent& event)
                                { return kEventResult_Continue; }

    virtual EventResult     HandleAxis(const AxisEvent& event)
                                { return kEventResult_Continue; }

    virtual void            HandleTextInput(const TextInputEvent& event)
                                {}

private:
    Priority                mPriority;
    bool                    mRegistered;

    friend class InputManager;
};
