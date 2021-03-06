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

#include "Core/String.h"

#include "Input/InputInfo.h"

/** Base input event structure. */
struct InputEvent
{
    /** Input that was performed. */
    InputCode               code;
    const InputInfo*        info;

    /** Current modifier state. */
    InputModifier           modifiers;

protected:
                            InputEvent(const InputInfo* const inInfo,
                                       const InputModifier    inModifiers);

};

inline InputEvent::InputEvent(const InputInfo* const inInfo,
                              const InputModifier    inModifiers) :
    code        (inInfo->code),
    info        (inInfo),
    modifiers   (inModifiers)
{
}

/** Details of a button up/down event. */
struct ButtonEvent : InputEvent
{
    bool                    down;

    /**
     * This gives a textual representation, if any, of a button pressed. While
     * the raw input codes correspond to physical key positions, irrespective
     * of layout, this gives the representation of the key for the user's
     * keyboard layout. If a key has no textual representation, this will be 0.
     */
    char                    character;

public:
                            ButtonEvent(const InputInfo* const inInfo,
                                        const InputModifier    inModifiers,
                                        const bool             inDown,
                                        const char             inCharacter);

};

inline ButtonEvent::ButtonEvent(const InputInfo* const inInfo,
                                const InputModifier    inModifiers,
                                const bool             inDown,
                                const char             inCharacter) :
    InputEvent  (inInfo, inModifiers),
    down        (inDown),
    character   (inCharacter)
{
}

/** Details of a axis movement event. */
struct AxisEvent : InputEvent
{
    /**
     * This gives the delta change on the axis. Scale of this value depends on
     * the type of axis. For mouse movement, it gives the delta change in
     * pixels. For mouse scrolling, it gives the number of positions scrolled
     * (positive is up, negative is down).
     */
    float                   delta;

public:
                            AxisEvent(const InputInfo* const inInfo,
                                      const InputModifier    inModifiers,
                                      const float            inDelta);

};

inline AxisEvent::AxisEvent(const InputInfo* const inInfo,
                            const InputModifier    inModifiers,
                            const float            inDelta) :
    InputEvent  (inInfo, inModifiers),
    delta       (inDelta)
{
}

/** Details of a text input event. */
struct TextInputEvent
{
    /** Text that was input. */
    std::string             text;

public:
                            TextInputEvent(std::string inText);

};

inline TextInputEvent::TextInputEvent(std::string inText) :
    text    (std::move(inText))
{
}
