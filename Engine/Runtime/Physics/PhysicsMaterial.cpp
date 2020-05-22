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

#include "Physics/PhysicsMaterial.h"

#include "Physics/PhysicsInternal.h"

PhysicsMaterial::PhysicsMaterial() :
    mRestitution (0.6f),
    mFriction    (0.5f)
{
}

PhysicsMaterial::~PhysicsMaterial()
{
}

void PhysicsMaterial::SetRestitution(const float restitution)
{
    Assert(restitution >= 0.0f && restitution <= 1.0f);

    mRestitution = restitution;
}

void PhysicsMaterial::SetFriction(const float friction)
{
    Assert(friction >= 0.0f);

    mFriction = friction;
}
