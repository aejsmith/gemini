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

#include "Core/CoreDefs.h"

#include <glm.h>

/**
 * Type aliases for angles to make clear which unit is expected. Both are
 * currently just aliases for float, this is mainly for documentation purposes.
 *
 * In general, degrees should be used for user-facing properties (e.g. exposed
 * through UI for editing), as degrees are more familiar. Radians are used for
 * everything else internally, as most math functions work with radians rather
 * than degrees.
 */
using Degrees = float;
using Radians = float;

namespace Math
{
    /**
     * Given two quaternions, a and b, returns the quaternion d such that
     * d * a = b.
     */
    static inline glm::quat QuatDifference(const glm::quat& a, const glm::quat& b)
    {
        return b * glm::inverse(a);
    }
}
