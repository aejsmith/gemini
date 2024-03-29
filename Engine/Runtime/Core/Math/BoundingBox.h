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

class Transform;

/**
 * Class definining an axis-aligned bounding box (AABB).
 */
class BoundingBox
{
public:
                                BoundingBox();
                                BoundingBox(const glm::vec3& minimum,
                                            const glm::vec3& maximum);

    const glm::vec3&            GetMinimum() const  { return mMinimum; }
    const glm::vec3&            GetMaximum() const  { return mMaximum; }

    /**
     * Gets the positive (P-) vertex for this box given a normal, i.e. the
     * vertex of the box which is furthest along the normal's direction.
     */
    glm::vec3                   CalculatePVertex(const glm::vec3& normal) const;

    /**
     * Gets the negative (N-) vertex for this box given a normal, i.e. the
     * vertex of the box which is furthest away from the normal's direction.
     */
    glm::vec3                   CalculateNVertex(const glm::vec3& normal) const;

    BoundingBox                 Transform(const glm::mat4& matrix) const;
    BoundingBox                 Transform(const class Transform& transform) const;

private:
    glm::vec3                   mMinimum;
    glm::vec3                   mMaximum;

};

inline BoundingBox::BoundingBox() :
    mMinimum    (0.0f),
    mMaximum    (0.0f)
{
}

inline BoundingBox::BoundingBox(const glm::vec3& minimum,
                                const glm::vec3& maximum) :
    mMinimum    (minimum),
    mMaximum    (maximum)
{
    Assert(maximum.x >= minimum.x &&
           maximum.y >= minimum.y &&
           maximum.z >= minimum.z);
}

inline bool operator==(const BoundingBox& a, const BoundingBox& b)
{
    return a.GetMinimum() == b.GetMinimum() && a.GetMaximum() == b.GetMaximum();
}

inline bool operator!=(const BoundingBox& a, const BoundingBox& b)
{
    return !(a == b);
}
