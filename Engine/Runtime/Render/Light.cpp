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

#include "Render/Light.h"

#include "Entity/World.h"

#include "Render/RenderWorld.h"

Light::Light() :
    mType           (kLightType_Point),
    mColour         (1.0f, 1.0f, 1.0f),
    mIntensity      (1.0f),
    mRange          (10.0f),
    mInnerConeAngle (35.0f),
    mOuterConeAngle (45.0f)
{
    mRenderLight.SetType(mType);
    mRenderLight.SetColour(mColour);
    mRenderLight.SetIntensity(mIntensity);
    mRenderLight.SetRange(mRange);
    mRenderLight.SetConeAngles(glm::radians(mInnerConeAngle), glm::radians(mOuterConeAngle));
}

Light::~Light()
{
}

void Light::SetType(const LightType inType)
{
    if (inType != mType)
    {
        mType = inType;
        mRenderLight.SetType(inType);
    }
}

void Light::SetColour(const glm::vec3& inColour)
{
    if (inColour != mColour)
    {
        mColour = inColour;
        mRenderLight.SetColour(inColour);
    }
}

void Light::SetIntensity(const float inIntensity)
{
    Assert(inIntensity >= 0.0f);

    if (inIntensity != mIntensity)
    {
        mIntensity = inIntensity;
        mRenderLight.SetIntensity(inIntensity);
    }
}

void Light::SetRange(const float inRange)
{
    Assert(inRange >= 0.0f);

    if (inRange != mRange)
    {
        mRange = inRange;
        mRenderLight.SetRange(inRange);
    }
}

void Light::SetInnerConeAngle(const float inInnerConeAngle)
{
    Assert(inInnerConeAngle >= 0.0f && inInnerConeAngle <= 90.0f);

    const float clampedAngle = glm::clamp(inInnerConeAngle, 0.0f, mOuterConeAngle);

    if (clampedAngle != mInnerConeAngle)
    {
        mInnerConeAngle = clampedAngle;
        mRenderLight.SetConeAngles(glm::radians(mInnerConeAngle), glm::radians(mOuterConeAngle));
    }
}

void Light::SetOuterConeAngle(const float inOuterConeAngle)
{
    Assert(inOuterConeAngle >= 0.0f && inOuterConeAngle <= 90.0f);

    const float clampedAngle = glm::clamp(inOuterConeAngle, mInnerConeAngle, 90.0f);

    if (clampedAngle != mOuterConeAngle)
    {
        mOuterConeAngle = clampedAngle;
        mRenderLight.SetConeAngles(glm::radians(mInnerConeAngle), glm::radians(mOuterConeAngle));
    }
}

void Light::Activated()
{
    RenderWorld* const renderWorld = GetEntity()->GetWorld()->GetRenderWorld();
    renderWorld->AddLight(&mRenderLight);
}

void Light::Deactivated()
{
    RenderWorld* const renderWorld = GetEntity()->GetWorld()->GetRenderWorld();
    renderWorld->RemoveLight(&mRenderLight);
}

void Light::Transformed()
{
    mRenderLight.SetPosition(GetWorldPosition());

    /* Light direction is the local negative Z axis. */
    const glm::vec3 kBaseDirection(0.0f, 0.0f, -1.0f);
    mRenderLight.SetDirection(GetWorldOrientation() * kBaseDirection);
}