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

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma GCC diagnostic ignored "-Wreorder-ctor"
#endif

#include <btBulletDynamicsCommon.h>

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#include "Core/Math.h"

#include "Engine/Profiler.h"

#define PHYSICS_PROFILER_SCOPE(timer)   PROFILER_SCOPE("Physics", timer, 0x507fff)
#define PHYSICS_PROFILER_FUNC_SCOPE()   PROFILER_FUNC_SCOPE("Physics", 0x507fff)

/**
 * Bullet helper functions (conversions, etc.)
 */
namespace BulletUtil
{
    static inline btVector3 ToBullet(const glm::vec3& vector)
    {
        return btVector3(vector.x, vector.y, vector.z);
    }

    static inline glm::vec3 FromBullet(const btVector3& vector)
    {
        return glm::vec3(vector.x(), vector.y(), vector.z());
    }

    static inline btQuaternion ToBullet(const glm::quat& quat)
    {
        return btQuaternion(quat.x, quat.y, quat.z, quat.w);
    }

    static inline glm::quat FromBullet(const btQuaternion& quat)
    {
        return glm::quat(quat.w(), quat.x(), quat.y(), quat.z());
    }
}
