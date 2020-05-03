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

#include "PlayerController.h"

#include "Input/InputManager.h"

static constexpr float kMovementVelocity = 5.0f;

PlayerController::PlayerController() :
    InputHandler    (InputHandler::kPriority_World),
    mDirection      (0.0f),
    mIsRotating     (false)
{
}

PlayerController::~PlayerController()
{
}

void PlayerController::Activated()
{
    RegisterInputHandler();
}

void PlayerController::Deactivated()
{
    UnregisterInputHandler();
}

void PlayerController::Tick(const float delta)
{
    const glm::vec3 movement = delta * kMovementVelocity * mDirection;
    GetEntity()->Translate(glm::vec3(0.0f, movement.y, 0.0f));
    GetEntity()->Translate(this->camera->GetWorldOrientation() * glm::vec3(movement.x, 0.0f, movement.z));
}

InputHandler::EventResult PlayerController::HandleButton(const ButtonEvent& event)
{
    glm::vec3 direction(0.0f);

    switch (event.code)
    {
        case kInputCode_W:
            direction.z = -1.0f;
            break;
        case kInputCode_S:
            direction.z =  1.0f;
            break;
        case kInputCode_A:
            direction.x = -1.0f;
            break;
        case kInputCode_D:
            direction.x =  1.0f;
            break;
        case kInputCode_LeftCtrl:
            direction.y = -1.0f;
            break;
        case kInputCode_Space:
            direction.y =  1.0f;
            break;

        case kInputCode_MouseRight:
            mIsRotating = event.down;
            break;

        default:
            break;

    }

    if (event.down)
    {
        mDirection += direction;
    }
    else
    {
        mDirection -= direction;
    }

    return kEventResult_Continue;
}

InputHandler::EventResult PlayerController::HandleAxis(const AxisEvent& event)
{
    if (mIsRotating)
    {
        switch (event.code)
        {
            case kInputCode_MouseX:
                GetEntity()->Rotate(-event.delta / 4, glm::vec3(0.0f, 1.0f, 0.0f));
                break;

            case kInputCode_MouseY:
                this->camera->GetEntity()->Rotate(-event.delta / 4, glm::vec3(1.0f, 0.0f, 0.0f));
                break;

            default:
                break;

        }
    }

    return kEventResult_Continue;
}
