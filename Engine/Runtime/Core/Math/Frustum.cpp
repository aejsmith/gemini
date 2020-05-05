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

#include "Core/Math/Frustum.h"

Frustum::Frustum(const glm::mat4& viewProjection,
                 const glm::mat4& inverseViewProjection)
{
    /* http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf */

    glm::vec4 planes[kNumPlanes];

    planes[kPlane_Left].x   =  viewProjection[0][3] + viewProjection[0][0];
    planes[kPlane_Left].y   =  viewProjection[1][3] + viewProjection[1][0];
    planes[kPlane_Left].z   =  viewProjection[2][3] + viewProjection[2][0];
    planes[kPlane_Left].w   = -viewProjection[3][3] - viewProjection[3][0];

    planes[kPlane_Right].x  =  viewProjection[0][3] - viewProjection[0][0];
    planes[kPlane_Right].y  =  viewProjection[1][3] - viewProjection[1][0];
    planes[kPlane_Right].z  =  viewProjection[2][3] - viewProjection[2][0];
    planes[kPlane_Right].w  = -viewProjection[3][3] + viewProjection[3][0];

    planes[kPlane_Top].x    =  viewProjection[0][3] - viewProjection[0][1];
    planes[kPlane_Top].y    =  viewProjection[1][3] - viewProjection[1][1];
    planes[kPlane_Top].z    =  viewProjection[2][3] - viewProjection[2][1];
    planes[kPlane_Top].w    = -viewProjection[3][3] + viewProjection[3][1];

    planes[kPlane_Bottom].x =  viewProjection[0][3] + viewProjection[0][1];
    planes[kPlane_Bottom].y =  viewProjection[1][3] + viewProjection[1][1];
    planes[kPlane_Bottom].z =  viewProjection[2][3] + viewProjection[2][1];
    planes[kPlane_Bottom].w = -viewProjection[3][3] - viewProjection[3][1];

    planes[kPlane_Near].x   =  viewProjection[0][2];
    planes[kPlane_Near].y   =  viewProjection[1][2];
    planes[kPlane_Near].z   =  viewProjection[2][2];
    planes[kPlane_Near].w   = -viewProjection[3][2];

    planes[kPlane_Far].x    =  viewProjection[0][3] - viewProjection[0][2];
    planes[kPlane_Far].y    =  viewProjection[1][3] - viewProjection[1][2];
    planes[kPlane_Far].z    =  viewProjection[2][3] - viewProjection[2][2];
    planes[kPlane_Far].w    = -viewProjection[3][3] + viewProjection[3][2];

    /* Normalize planes. */
    for (unsigned i = 0; i < kNumPlanes; i++)
    {
        const glm::vec4& plane = planes[i];
        const float magSquared = (plane.x * plane.x) + (plane.y * plane.y) + (plane.z * plane.z);
        mPlanes[i]             = plane * glm::inversesqrt(magSquared);
    }

    /* Calculate corners. */
    const glm::vec3 corners[kNumCorners] =
    {
        glm::vec3(-1.0f,  1.0f, -1.0f),
        glm::vec3( 1.0f,  1.0f, -1.0f),
        glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3( 1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f,  1.0f,  1.0f),
        glm::vec3( 1.0f,  1.0f,  1.0f),
        glm::vec3(-1.0f, -1.0f,  1.0f),
        glm::vec3( 1.0f, -1.0f,  1.0f),
    };

    for (unsigned i = 0; i < kNumCorners; i++)
    {
        const glm::vec4 inverted = inverseViewProjection * glm::vec4(corners[i], 1.0f);
        mCorners[i]              = glm::vec3(inverted) / inverted.w;
    }
}
