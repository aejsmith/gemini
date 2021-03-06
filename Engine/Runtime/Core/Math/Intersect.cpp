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

#include "Core/Math/Intersect.h"

bool Math::Intersect(const Frustum& frustum,
                     const Sphere&  sphere)
{
    for (unsigned i = 0; i < Frustum::kNumPlanes; i++)
    {
        const Plane& plane = frustum.GetPlane(i);

        /* Plane normals point inside the frustum. DistanceTo() is negative
         * when the point is behind the plane. */
        if (plane.DistanceTo(sphere.GetCentre()) < -sphere.GetRadius())
        {
            return false;
        }
    }

    return true;
}

bool Math::Intersect(const Frustum&     frustum,
                     const BoundingBox& box)
{
    /*
     * TODO: There is inaccuracy here with larger AABBs. If the AABB intersects
     * with one of the planes but the point of intersection is not actually
     * within the frustum we can receive a false positive result.
     */

    for (unsigned i = 0; i < Frustum::kNumPlanes; i++)
    {
        const Plane& plane = frustum.GetPlane(i);
        const glm::vec3 p  = box.CalculatePVertex(plane.GetNormal());

        if (plane.DistanceTo(p) < 0.0f)
        {
            return false;
        }
    }

    return true;
}
