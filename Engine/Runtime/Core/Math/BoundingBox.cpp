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

#include "Core/Math/BoundingBox.h"

glm::vec3 BoundingBox::CalculatePVertex(const glm::vec3& inNormal) const
{
    glm::vec3 p = mMinimum;

    if (inNormal.x >= 0.0f)
    {
        p.x = mMaximum.x;
    }

    if (inNormal.y >= 0.0f)
    {
        p.y = mMaximum.y;
    }

    if (inNormal.z >= 0.0f)
    {
        p.z = mMaximum.z;
    }

    return p;
}

glm::vec3 BoundingBox::CalculateNVertex(const glm::vec3& inNormal) const
{
    glm::vec3 n = mMaximum;

    if (inNormal.x >= 0.0f)
    {
        n.x = mMinimum.x;
    }

    if (inNormal.y >= 0.0f)
    {
        n.y = mMinimum.y;
    }

    if (inNormal.z >= 0.0f)
    {
        n.z = mMinimum.z;
    }

    return n;
}

BoundingBox BoundingBox::Transform(const glm::mat4& inMatrix) const
{
    glm::vec3 xa = glm::vec3(inMatrix[0]) * mMinimum.x;
    glm::vec3 xb = glm::vec3(inMatrix[0]) * mMaximum.x;

    glm::vec3 ya = glm::vec3(inMatrix[1]) * mMinimum.y;
    glm::vec3 yb = glm::vec3(inMatrix[1]) * mMaximum.y;

    glm::vec3 za = glm::vec3(inMatrix[2]) * mMinimum.z;
    glm::vec3 zb = glm::vec3(inMatrix[2]) * mMaximum.z;

    glm::vec3 minimum(glm::min(xa, xb) + glm::min(ya, yb) + glm::min(za, zb) + glm::vec3(inMatrix[3]));
    glm::vec3 maximum(glm::max(xa, xb) + glm::max(ya, yb) + glm::max(za, zb) + glm::vec3(inMatrix[3]));

    return BoundingBox(minimum, maximum);
}
