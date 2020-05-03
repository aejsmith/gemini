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

#include "Core/Math/Sphere.h"

class Cone
{
public:
                                Cone();
                                Cone(const glm::vec3& origin,
                                     const glm::vec3& direction,
                                     const float      height,
                                     const Radians    halfAngle);

    const glm::vec3&            GetOrigin() const       { return mOrigin; }
    const glm::vec3&            GetDirection() const    { return mDirection; }
    float                       GetHeight() const       { return mHeight; }
    Radians                     GetHalfAngle() const    { return mHalfAngle; }

    /**
     * Generate geometry (triangle list) representing the cone. baseVertices
     * specifies the number of divisions around the base.
     */
    void                        CreateGeometry(const uint32_t          baseVertices,
                                               std::vector<glm::vec3>& outVertices,
                                               std::vector<uint16_t>&  outIndices) const;

    /**
     * Calculate a bounding sphere that fits the cone as tightly as possible.
     * This will start to become very inaccurate for half angles greater than
     * 45 degrees.
     */
    Sphere                      CalculateBoundingSphere() const;

private:
    glm::vec3                   mOrigin;
    glm::vec3                   mDirection;
    float                       mHeight;
    Radians                     mHalfAngle;

};

inline Cone::Cone() :
    mOrigin     (0.0f),
    mDirection  (0.0f),
    mHeight     (0.0f),
    mHalfAngle  (0.0f)
{
}

inline Cone::Cone(const glm::vec3& origin,
                  const glm::vec3& direction,
                  const float      height,
                  const Radians    halfAngle) :
    mOrigin     (origin),
    mDirection  (direction),
    mHeight     (height),
    mHalfAngle  (halfAngle)
{
    Assert(glm::isNormalized(direction, 0.0001f));
}
