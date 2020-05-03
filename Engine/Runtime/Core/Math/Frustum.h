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

#include "Core/Math/Plane.h"

/**
 * This class encorporates some utility functionality for frustums. It does not
 * include functionality for defining a frustum and its matrices, rather it
 * takes pre-calculated view/projection matrices and converts them to a plane
 * representation in order to peform intersection tests, etc. Note that the
 * positive half-space of each plane is inside the frustum.
 */
class Frustum
{
public:
    enum
    {
        kPlane_Left,
        kPlane_Right,
        kPlane_Top,
        kPlane_Bottom,
        kPlane_Near,
        kPlane_Far,

        kNumPlanes,
    };

    enum
    {
        kCorner_NearTopLeft,
        kCorner_NearTopRight,
        kCorner_NearBottomLeft,
        kCorner_NearBottomRight,
        kCorner_FarTopLeft,
        kCorner_FarTopRight,
        kCorner_FarBottomLeft,
        kCorner_FarBottomRight,

        kNumCorners,
    };

public:
                            Frustum() {}
                            Frustum(const glm::mat4& viewProjection,
                                    const glm::mat4& inverseViewProjection);

    const Plane&            GetPlane(const int plane) const
                                { return mPlanes[plane]; }

    const glm::vec3&        GetCorner(const int corner) const
                                { return mCorners[corner]; }

private:
    Plane                   mPlanes[kNumPlanes];
    glm::vec3               mCorners[kNumCorners];
};
