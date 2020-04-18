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

#include "Core/IntrusiveList.h"
#include "Core/Math/Frustum.h"
#include "Core/Math/Sphere.h"

#include "Render/RenderDefs.h"

struct LightParams;

/**
 * Internal implementation of a light.
 * 
 * This is a separate class to Light, both to keep the renderer internal
 * implementation details separate from the public interface, and in
 * preparation for future multithreading support which would allow rendering
 * and the entity system tick to run in parallel (keep entity system and
 * renderer state separate, with a synchronisation point early in the frame).
 */
class RenderLight
{
public:
                                RenderLight();
                                ~RenderLight();

    LightType                   GetType() const { return mType; }
    void                        SetType(const LightType inType);

    const glm::vec3&            GetColour() const { return mColour; }
    void                        SetColour(const glm::vec3& inColour);

    float                       GetIntensity() const { return mIntensity; }
    void                        SetIntensity(const float inIntensity);

    float                       GetRange() const { return mRange; }
    void                        SetRange(const float inRange);

    float                       GetConeAngleScale() const  { return mConeAngleScale; }
    float                       GetConeAngleOffset() const { return mConeAngleOffset; }
    void                        SetConeAngles(const Radians inInnerConeAngle,
                                              const Radians inOuterConeAngle);

    const glm::vec3&            GetPosition() const { return mPosition; }
    void                        SetPosition(const glm::vec3& inPosition);

    const glm::vec3&            GetDirection() const { return mDirection; }
    void                        SetDirection(const glm::vec3& inDirection);

    /** Returns whether the light's area of effect intersects with a frustum. */
    bool                        Cull(const Frustum& inFrustum) const;

    /** Fill in a shader LightParams structure for the light. */
    void                        GetLightParams(LightParams& outParams) const;

    /** Draw the light to the debug overlay. */
    void                        DrawDebugPrimitive() const;

private:
    void                        UpdateBoundingSphere();

private:
    LightType                   mType;
    glm::vec3                   mColour;
    float                       mIntensity;
    float                       mRange;

    /** Cone angle parameters (radians). */
    Radians                     mConeAngle;
    float                       mConeAngleScale;
    float                       mConeAngleOffset;

    /** World space transformation. */
    glm::vec3                   mPosition;
    glm::vec3                   mDirection;

    /**
     * Bounding sphere. Exact for point lights, fitted around cone for spot
     * lights.
     */
    Sphere                      mBoundingSphere;

public:
    IntrusiveListNode           mWorldListNode;

};
