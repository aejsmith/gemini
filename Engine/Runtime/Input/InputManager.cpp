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

#include "Input/InputManager.h"

#include "Input/InputHandler.h"

#include <functional>

#include <SDL.h>

SINGLETON_IMPL(InputManager);

InputManager::InputManager() :
    mMouseCaptured      (false),
    mTextInputHandler   (nullptr)
{
    SDL_StopTextInput();
}

InputModifier InputManager::GetModifiers()
{
    SDL_Keymod sdlModifiers = SDL_GetModState();

    InputModifier modifiers = kInputModifier_None;

    if (sdlModifiers & KMOD_LSHIFT) modifiers |= kInputModifier_LeftShift;
    if (sdlModifiers & KMOD_RSHIFT) modifiers |= kInputModifier_RightShift;
    if (sdlModifiers & KMOD_LCTRL)  modifiers |= kInputModifier_LeftCtrl;
    if (sdlModifiers & KMOD_RCTRL)  modifiers |= kInputModifier_RightCtrl;
    if (sdlModifiers & KMOD_LALT)   modifiers |= kInputModifier_LeftAlt;
    if (sdlModifiers & KMOD_RALT)   modifiers |= kInputModifier_RightAlt;
    if (sdlModifiers & KMOD_LGUI)   modifiers |= kInputModifier_LeftSuper;
    if (sdlModifiers & KMOD_RGUI)   modifiers |= kInputModifier_RightSuper;
    if (sdlModifiers & KMOD_NUM)    modifiers |= kInputModifier_NumLock;
    if (sdlModifiers & KMOD_CAPS)   modifiers |= kInputModifier_CapsLock;

    return modifiers;
}

bool InputManager::GetButtonState(const InputCode code)
{
    const InputInfo* inputInfo = InputInfo::Lookup(code);

    AssertMsg(inputInfo, "Input code %d is invalid", code);
    AssertMsg(inputInfo->type == kInputType_Button, "Input %d is not a button", code);
    Unused(inputInfo);

    if (code >= kInputCodeKeyboardFirst && code <= kInputCodeKeyboardLast)
    {
        const uint8_t* const keyboardState = SDL_GetKeyboardState(nullptr);
        return keyboardState[code];
    }
    else if (code >= kInputCodeMouseFirst && code <= kInputCodeMouseLast)
    {
        const uint32_t mouseState = SDL_GetMouseState(nullptr, nullptr);

        switch (code)
        {
            case kInputCode_MouseLeft:
                return mouseState & SDL_BUTTON(SDL_BUTTON_LEFT);

            case kInputCode_MouseMiddle:
                return mouseState & SDL_BUTTON(SDL_BUTTON_MIDDLE);

            case kInputCode_MouseRight:
                return mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT);

            default:
                Unreachable();

        }
    }
    else
    {
        Unreachable();
    }
}

glm::ivec2 InputManager::GetCursorPosition()
{
    int x, y;
    SDL_GetMouseState(&x, &y);
    return glm::ivec2(x, y);
}

void InputManager::SetMouseCaptured(const bool captured)
{
    if (mMouseCaptured != captured)
    {
        SDL_SetRelativeMouseMode((captured) ? SDL_TRUE : SDL_FALSE);
        mMouseCaptured = captured;
    }
}

void InputManager::RegisterHandler(InputHandler* const handler,
                                   OnlyCalledBy<InputHandler>)
{
    /* List is sorted by priority. */
    for (auto it = mHandlers.begin(); it != mHandlers.end(); ++it)
    {
        const InputHandler* const other = *it;

        if (handler->GetInputPriority() < other->GetInputPriority())
        {
            mHandlers.insert(it, handler);
            return;
        }
    }

    /* Insertion point not found, add at end. */
    mHandlers.push_back(handler);
}

void InputManager::UnregisterHandler(InputHandler* const handler,
                                     OnlyCalledBy<InputHandler>)
{
    mHandlers.remove(handler);
}

void InputManager::BeginTextInput(InputHandler* const handler,
                                  OnlyCalledBy<InputHandler>)
{
    AssertMsg(!mTextInputHandler, "Multiple input handlers requesting text input");

    mTextInputHandler = handler;
    SDL_StartTextInput();
}

void InputManager::EndTextInput(InputHandler* const handler,
                                OnlyCalledBy<InputHandler>)
{
    Assert(mTextInputHandler == handler);

    SDL_StopTextInput();
    mTextInputHandler = nullptr;
}

bool InputManager::HandleEvent(const SDL_Event& event,
                               OnlyCalledBy<Engine>)
{
    const InputModifier modifiers = GetModifiers();

    auto DispatchInputEvent =
        [&] (auto function)
        {
            for (InputHandler* handler : mHandlers)
            {
                if (function(handler) == InputHandler::kEventResult_Stop)
                {
                    break;
                }
            }
        };

    switch (event.type)
    {
        case SDL_KEYDOWN:
        {
            /* Ignore repeats for now. FIXME. */
            if (event.key.repeat)
            {
                return false;
            }

            /* Fallthrough. */
        }

        case SDL_KEYUP:
        {
            /* Map the scan code to an input code. */
            const InputCode inputCode        = static_cast<InputCode>(event.key.keysym.scancode);
            const InputInfo* const inputInfo = InputInfo::Lookup(inputCode);

            if (!inputInfo)
            {
                LogWarning("Unrecognised scan code %u", event.key.keysym.scancode);
                return false;
            }

            /* Get the character representation, if any, of this code. SDL's
             * keycodes are defined to ASCII values if they have a printable
             * representation, or the scancode with bit 30 set otherwise. */
            const char character = (!(event.key.keysym.sym & SDLK_SCANCODE_MASK))
                                       ? event.key.keysym.sym
                                       : 0;

            const ButtonEvent buttonEvent(inputInfo, modifiers, event.type == SDL_KEYDOWN, character);

            DispatchInputEvent(
                [&] (InputHandler* const handler)
                {
                    return handler->HandleButton(buttonEvent);
                });

            return true;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            /* Convert SDL's button to our own. */
            InputCode inputCode;
            switch (event.button.button)
            {
                case SDL_BUTTON_LEFT:
                    inputCode = kInputCode_MouseLeft;
                    break;

                case SDL_BUTTON_RIGHT:
                    inputCode = kInputCode_MouseRight;
                    break;

                case SDL_BUTTON_MIDDLE:
                    inputCode = kInputCode_MouseMiddle;
                    break;

                default:
                    LogWarning("Unrecognised SDL button code %u", event.button.button);
                    return false;

            }

            const InputInfo* const inputInfo = InputInfo::Lookup(inputCode);

            const ButtonEvent buttonEvent(inputInfo, modifiers, event.type == SDL_MOUSEBUTTONDOWN, 0);

            DispatchInputEvent(
                [&] (InputHandler* const handler)
                {
                    return handler->HandleButton(buttonEvent);
                });

            return true;
        }

        case SDL_MOUSEMOTION:
        {
            if (event.motion.xrel)
            {
                const InputInfo* const inputInfo = InputInfo::Lookup(kInputCode_MouseX);

                const AxisEvent axisEvent(inputInfo, modifiers, event.motion.xrel);

                DispatchInputEvent(
                    [&] (InputHandler* const handler)
                    {
                        return handler->HandleAxis(axisEvent);
                    });
            }

            if (event.motion.yrel)
            {
                const InputInfo* const inputInfo = InputInfo::Lookup(kInputCode_MouseY);

                const AxisEvent axisEvent(inputInfo, modifiers, event.motion.yrel);

                DispatchInputEvent(
                    [&] (InputHandler* const handler)
                    {
                        return handler->HandleAxis(axisEvent);
                    });
            }

            return true;
        }

        case SDL_MOUSEWHEEL:
        {
            const InputInfo* const inputInfo = InputInfo::Lookup(kInputCode_MouseScroll);

            const AxisEvent axisEvent(inputInfo, modifiers, event.wheel.y);

            DispatchInputEvent(
                [&] (InputHandler* const handler)
                {
                    return handler->HandleAxis(axisEvent);
                });

            return true;
        }

        case SDL_TEXTINPUT:
        {
            if (mTextInputHandler)
            {
                const TextInputEvent textInputEvent(event.text.text);

                mTextInputHandler->HandleTextInput(textInputEvent);
            }

            return true;
        }
    }

    return false;
}
