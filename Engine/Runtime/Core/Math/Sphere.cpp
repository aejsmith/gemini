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

void Sphere::CreateGeometry(const uint32_t          inRings,
                            const uint32_t          inSectors,
                            std::vector<glm::vec3>& outVertices,
                            std::vector<uint16_t>&  outIndices) const
{
    outVertices.clear();
    outVertices.reserve(inRings * inSectors);

    outIndices.clear();
    outIndices.reserve(inRings * inSectors * 6);

    const float R = 1.0f / static_cast<float>(inRings - 1);
    const float S = 1.0f / static_cast<float>(inSectors - 1);

    for (uint32_t r = 0; r < inRings; r++)
    {
        for (uint32_t s = 0; s < inSectors; s++)
        {
            const float x = cos(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);
            const float y = sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
            const float z = sin(2 * glm::pi<float>() * s * S) * sin(glm::pi<float>() * r * R);

            outVertices.emplace_back(GetCentre() + (glm::vec3(x, y, z) * GetRadius()));
        }
    }

    for (uint32_t r = 0; r < inRings - 1; r++)
    {
        for (uint32_t s = 0; s < inSectors - 1; s++)
        {
            outIndices.emplace_back(r * inSectors + s);
            outIndices.emplace_back((r + 1) * inSectors + s);
            outIndices.emplace_back((r + 1) * inSectors + (s + 1));
            outIndices.emplace_back((r + 1) * inSectors + (s + 1));
            outIndices.emplace_back(r * inSectors + (s + 1));
            outIndices.emplace_back(r * inSectors + s);
        }
    }
}
