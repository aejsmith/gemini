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

#include "Core/Math/Sphere.h"

void Sphere::CreateGeometry(const uint32_t          rings,
                            const uint32_t          sectors,
                            std::vector<glm::vec3>& outVertices,
                            std::vector<uint16_t>&  outIndices) const
{
    outVertices.clear();
    outVertices.reserve(rings * sectors);

    outIndices.clear();
    outIndices.reserve(rings * sectors * 6);

    const float R = 1.0f / static_cast<float>(rings - 1);
    const float S = 1.0f / static_cast<float>(sectors - 1);

    for (uint32_t r = 0; r < rings; r++)
    {
        for (uint32_t s = 0; s < sectors; s++)
        {
            const float x = cos(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);
            const float y = sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
            const float z = sin(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);

            outVertices.emplace_back(GetCentre() + (glm::vec3(x, y, z) * GetRadius()));
        }
    }

    for (uint32_t r = 0; r < rings - 1; r++)
    {
        for (uint32_t s = 0; s < sectors - 1; s++)
        {
            outIndices.emplace_back(r * sectors + s);
            outIndices.emplace_back((r + 1) * sectors + s);
            outIndices.emplace_back((r + 1) * sectors + (s + 1));
            outIndices.emplace_back((r + 1) * sectors + (s + 1));
            outIndices.emplace_back(r * sectors + (s + 1));
            outIndices.emplace_back(r * sectors + s);
        }
    }
}
