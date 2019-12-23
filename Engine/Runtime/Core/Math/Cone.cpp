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

#include "Core/Math/Cone.h"

#include "Core/Math/Transform.h"

#include <glm/gtx/quaternion.hpp>

void Cone::CreateGeometry(const uint32_t          inBaseVertices,
                          std::vector<glm::vec3>& outVertices,
                          std::vector<uint16_t>&  outIndices) const
{
    outVertices.clear();
    outVertices.reserve(inBaseVertices + 2);

    outIndices.clear();
    outIndices.reserve(6 * inBaseVertices);

    /* Calculate a transformation between a unit cone in the negative Z
     * direction and a cone with the properties we want. */
    constexpr glm::vec3 kBaseDirection(0.0f, 0.0f, -1.0f);
    const float radius = std::tan(mHalfAngle) * mHeight;
    const Transform transform(mOrigin,
                              glm::rotation(kBaseDirection, mDirection),
                              glm::vec3(radius, radius, mHeight));

    /* Add the vertices. Head, then the base. */
    outVertices.emplace_back(mOrigin);
    outVertices.emplace_back(mOrigin + (mDirection * mHeight));
    const float delta = (2 * glm::pi<float>()) / inBaseVertices;
    for (uint32_t i = 0; i < inBaseVertices; i++)
    {
        const float angle = i * delta;
        const float x = cosf(angle);
        const float y = sinf(angle);
        const float z = -1;
        outVertices.emplace_back(glm::vec3(transform.GetMatrix() * glm::vec4(x, y, z, 1.0f)));
    }

    /* Add indices. Cone head to base, then the base. */
    for (uint32_t i = 0; i < inBaseVertices; i++)
    {
        outIndices.emplace_back(0);
        outIndices.emplace_back(i + 2);
        outIndices.emplace_back(((i + 1) % inBaseVertices) + 2);
    }
    for (uint32_t i = 0; i < inBaseVertices; i++)
    {
        outIndices.emplace_back(1);
        outIndices.emplace_back(i + 2);
        outIndices.emplace_back(((i + 1) % inBaseVertices) + 2);
    }
}
