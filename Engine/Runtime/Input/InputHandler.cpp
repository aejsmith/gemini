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

#include "Input/InputHandler.h"

#include "Input/InputManager.h"

InputHandler::InputHandler(const Priority inPriority) :
    mPriority   (inPriority),
    mRegistered (false)
{
}

InputHandler::~InputHandler()
{
    if (mRegistered)
    {
        UnregisterInputHandler();
    }
}

void InputHandler::SetInputPriority(const Priority inPriority)
{
    if (mRegistered)
    {
        UnregisterInputHandler();
        mPriority = inPriority;
        RegisterInputHandler();
    }
    else
    {
        mPriority = inPriority;
    }
}

void InputHandler::RegisterInputHandler()
{
    Assert(!mRegistered);

    InputManager::Get().RegisterHandler(this, {});
    mRegistered = true;
}

void InputHandler::UnregisterInputHandler()
{
    Assert(mRegistered);

    InputManager::Get().UnregisterHandler(this, {});
    mRegistered = false;
}

void InputHandler::BeginTextInput()
{
    InputManager::Get().BeginTextInput(this, {});
}

void InputHandler::EndTextInput()
{
    InputManager::Get().EndTextInput(this, {});
}
