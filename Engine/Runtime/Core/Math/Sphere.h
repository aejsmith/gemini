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

class Sphere
{
public:
                                Sphere();
                                Sphere(const glm::vec3& inCentre,
                                       const float      inRadius);

    const glm::vec3&            GetCentre() const   { return mCentre; }
    float                       GetRadius() const   { return mRadius; }

private:
    glm::vec3                   mCentre;
    float                       mRadius;

};

inline Sphere::Sphere() :
    mCentre (0.0f),
    mRadius (0.0f)
{
}

inline Sphere::Sphere(const glm::vec3& inCentre,
                      const float      inRadius) :
    mCentre (inCentre),
    mRadius (inRadius)
{
    Assert(inRadius >= 0.0f);
}
