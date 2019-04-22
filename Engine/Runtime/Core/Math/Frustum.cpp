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

#include "Core/Math/Frustum.h"

Frustum::Frustum(const glm::mat4& inViewProjection,
                 const glm::mat4& inInverseViewProjection)
{
    /* http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf */

    glm::vec4 planes[kNumPlanes];

    planes[kPlane_Left].x   =  inViewProjection[0][3] + inViewProjection[0][0];
    planes[kPlane_Left].y   =  inViewProjection[1][3] + inViewProjection[1][0];
    planes[kPlane_Left].z   =  inViewProjection[2][3] + inViewProjection[2][0];
    planes[kPlane_Left].w   = -inViewProjection[3][3] - inViewProjection[3][0];

    planes[kPlane_Right].x  =  inViewProjection[0][3] - inViewProjection[0][0];
    planes[kPlane_Right].y  =  inViewProjection[1][3] - inViewProjection[1][0];
    planes[kPlane_Right].z  =  inViewProjection[2][3] - inViewProjection[2][0];
    planes[kPlane_Right].w  = -inViewProjection[3][3] + inViewProjection[3][0];

    planes[kPlane_Top].x    =  inViewProjection[0][3] - inViewProjection[0][1];
    planes[kPlane_Top].y    =  inViewProjection[1][3] - inViewProjection[1][1];
    planes[kPlane_Top].z    =  inViewProjection[2][3] - inViewProjection[2][1];
    planes[kPlane_Top].w    = -inViewProjection[3][3] + inViewProjection[3][1];

    planes[kPlane_Bottom].x =  inViewProjection[0][3] + inViewProjection[0][1];
    planes[kPlane_Bottom].y =  inViewProjection[1][3] + inViewProjection[1][1];
    planes[kPlane_Bottom].z =  inViewProjection[2][3] + inViewProjection[2][1];
    planes[kPlane_Bottom].w = -inViewProjection[3][3] - inViewProjection[3][1];

    planes[kPlane_Near].x   =  inViewProjection[0][3] + inViewProjection[0][2];
    planes[kPlane_Near].y   =  inViewProjection[1][3] + inViewProjection[1][2];
    planes[kPlane_Near].z   =  inViewProjection[2][3] + inViewProjection[2][2];
    planes[kPlane_Near].w   = -inViewProjection[3][3] - inViewProjection[3][2];

    planes[kPlane_Far].x    =  inViewProjection[0][3] - inViewProjection[0][2];
    planes[kPlane_Far].y    =  inViewProjection[1][3] - inViewProjection[1][2];
    planes[kPlane_Far].z    =  inViewProjection[2][3] - inViewProjection[2][2];
    planes[kPlane_Far].w    = -inViewProjection[3][3] + inViewProjection[3][2];

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
        const glm::vec4 inverted = inInverseViewProjection * glm::vec4(corners[i], 1.0f);
        mCorners[i]              = glm::vec3(inverted) / inverted.w;
    }
}
