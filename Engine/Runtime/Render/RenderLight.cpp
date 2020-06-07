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

#include "Render/RenderLight.h"

#include "Engine/DebugManager.h"

#include "Core/Math/Cone.h"
#include "Core/Math/Intersect.h"

#include "../../Shaders/LightingDefs.h"

RenderLight::RenderLight() :
    /* Parent Light object should initialise everything else. */
    mPosition   (0.0f, 0.0f, 0.0f),
    mDirection  (0.0f, 0.0f, -1.0f)
{
}

RenderLight::~RenderLight()
{
}

void RenderLight::SetType(const LightType type)
{
    mType = type;
}

void RenderLight::SetColour(const glm::vec3& colour)
{
    mColour = colour;
}

void RenderLight::SetIntensity(const float intensity)
{
    mIntensity = intensity;
}

void RenderLight::SetRange(const float range)
{
    mRange = range;

    UpdateBoundingSphere();
}

void RenderLight::SetConeAngles(const Radians innerConeAngle,
                                const Radians outerConeAngle)
{
    mConeAngle       = outerConeAngle;
    mConeAngleScale  = 1.0f / std::max(0.001f, std::cos(innerConeAngle) - std::cos(outerConeAngle));
    mConeAngleOffset = -std::cos(outerConeAngle) * mConeAngleScale;

    UpdateBoundingSphere();
}

void RenderLight::SetPosition(const glm::vec3& position)
{
    mPosition = position;

    UpdateBoundingSphere();
}

void RenderLight::SetDirection(const glm::vec3& direction)
{
    mDirection = glm::normalize(direction);

    UpdateBoundingSphere();
}

void RenderLight::SetCastShadows(const bool castShadows)
{
    mCastShadows = castShadows;
}

bool RenderLight::Cull(const Frustum& frustum) const
{
    /* TODO: Improve zero range culling for spot lights, should still be
     * possible to test if the cone won't intersect the view frustum. */
    if (mType == kLightType_Directional || mRange == 0.0f)
    {
        return true;
    }
    else
    {
        return Math::Intersect(mBoundingSphere, frustum);
    }
}

void RenderLight::GetLightParams(LightParams& outParams) const
{
    static_assert(kLightType_Directional == kShaderLightType_Directional);
    static_assert(kLightType_Point       == kShaderLightType_Point);
    static_assert(kLightType_Spot        == kShaderLightType_Spot);

    outParams.position        = mPosition;
    outParams.type            = mType;
    outParams.direction       = mDirection;
    outParams.range           = mRange;
    outParams.colour          = mColour;
    outParams.intensity       = mIntensity;
    outParams.spotAngleScale  = mConeAngleScale;
    outParams.spotAngleOffset = mConeAngleOffset;
    outParams.shadowMaskIndex = kShaderLight_NoShadows;
    outParams._pad0           = 0.0f;
    outParams.boundingSphere  = glm::vec4(mBoundingSphere.GetCentre(), mBoundingSphere.GetRadius());
}

void RenderLight::UpdateBoundingSphere()
{
    if (mType == kLightType_Point)
    {
        mBoundingSphere = Sphere(mPosition, mRange);
    }
    else
    {
        /* Fit a sphere around the spot light cone. */
        const Cone cone(mPosition, mDirection, mRange, mConeAngle);
        mBoundingSphere = cone.CalculateBoundingSphere();
    }
}

void RenderLight::DrawDebugPrimitive() const
{
    static constexpr glm::vec3 kLightColour(0.0f, 1.0f, 0.0f);

    switch (mType)
    {
        case kLightType_Directional:
        {
            /* Directional lights have an effectively infinite bounding box,
             * don't do anything. */
            break;
        }

        case kLightType_Point:
        {
            DebugManager::Get().DrawPrimitive(mBoundingSphere, kLightColour);
            break;
        }

        case kLightType_Spot:
        {
            const Cone cone(mPosition, mDirection, mRange, mConeAngle);
            DebugManager::Get().DrawPrimitive(cone, kLightColour);
            break;
        }

        default:
            Unreachable();

    }
}
