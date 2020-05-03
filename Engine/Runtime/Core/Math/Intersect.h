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

#include "Core/Math/BoundingBox.h"
#include "Core/Math/Frustum.h"
#include "Core/Math/Sphere.h"

namespace Math
{
    bool                    Intersect(const Frustum&     frustum,
                                      const Sphere&      sphere);

    bool                    Intersect(const Sphere&      sphere,
                                      const Frustum&     frustum);

    bool                    Intersect(const Frustum&     frustum,
                                      const BoundingBox& box);

    bool                    Intersect(const BoundingBox& box,
                                      const Frustum&     frustum);
}

inline bool Math::Intersect(const Sphere&  sphere,
                            const Frustum& frustum)
{
    return Intersect(frustum, sphere);
}

inline bool Math::Intersect(const BoundingBox& box,
                            const Frustum&     frustum)
{
    return Intersect(frustum, box);
}
