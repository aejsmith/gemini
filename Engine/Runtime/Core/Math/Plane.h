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

#include "Core/Math.h"

/**
 * This class represents a plane in 3D space as a normal vector plus a distance
 * from the origin to the plane. The side away from which the normal points is
 * the positive half-space. The distance from the origin is in the direction
 * of the normal.
 */
class Plane
{
public:
                                Plane();
                                Plane(const glm::vec4& vector);
                                Plane(const glm::vec3& normal,
                                      const float      distance);
                                Plane(const glm::vec3& normal,
                                      const glm::vec3& point);

    /** Get the vector representation of the plane (normal, distance). */
    const glm::vec4&            GetVector() const   { return mVector; }

    /** Get the plane normal. */
    glm::vec3                   GetNormal() const   { return glm::vec3(mVector); }

    /** Get the distance from the origin to the plane in the normal direction. */
    float                       GetDistance() const { return mVector.w; }

    /**
     * Get the signed distance to a point from the plane. Positive if in front
     * of the plane, i.e. in the direction of the normal.
     */
    float                       DistanceTo(const glm::vec3& point) const;

private:
    glm::vec4                   mVector;
};

inline Plane::Plane() :
    mVector (0.0f)
{
}

inline Plane::Plane(const glm::vec4& vector) :
    mVector (vector)
{
}

inline Plane::Plane(const glm::vec3& normal,
                    const float      distance) :
    mVector (normal, distance)
{
    Assert(glm::isNormalized(normal, FLT_EPSILON));
}

inline Plane::Plane(const glm::vec3& normal,
                    const glm::vec3& point) :
    mVector (normal, glm::dot(normal, point))
{
    Assert(glm::isNormalized(normal, FLT_EPSILON));
}

inline float Plane::DistanceTo(const glm::vec3& point) const
{
    return glm::dot(glm::vec3(mVector), point) - mVector.w;
}
