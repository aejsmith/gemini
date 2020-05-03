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

#include "Core/Math.h"

#include <vector>

class Sphere
{
public:
                                Sphere();
                                Sphere(const glm::vec3& centre,
                                       const float      radius);

    const glm::vec3&            GetCentre() const   { return mCentre; }
    float                       GetRadius() const   { return mRadius; }

    /**
     * Generate geometry (triangle list) representing the sphere. rings
     * specifies the number of rings along the Y axis (like lines of latitude),
     * sectors specifies the number of rings around the Y axis (like lines of
     * longitude).
     */
    void                        CreateGeometry(const uint32_t          rings,
                                               const uint32_t          sectors,
                                               std::vector<glm::vec3>& outVertices,
                                               std::vector<uint16_t>&  outIndices) const;

private:
    glm::vec3                   mCentre;
    float                       mRadius;

};

inline Sphere::Sphere() :
    mCentre (0.0f),
    mRadius (0.0f)
{
}

inline Sphere::Sphere(const glm::vec3& centre,
                      const float      radius) :
    mCentre (centre),
    mRadius (radius)
{
    Assert(radius >= 0.0f);
}
