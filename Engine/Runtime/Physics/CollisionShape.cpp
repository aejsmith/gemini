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

#include "Physics/CollisionShape.h"

#include "Physics/PhysicsInternal.h"
#include "Physics/RigidBody.h"

CollisionShape::CollisionShape() :
    mCurrentWorldScale (0.0f, 0.0f, 0.0f),
    mRigidBody         (nullptr)
{
}

CollisionShape::~CollisionShape()
{
}

void CollisionShape::Activated()
{
    /* Look for the RigidBody we should become a part of. */
    Entity* entity = GetEntity();
    while (entity)
    {
        RigidBody* const rigidBody = entity->FindComponent<RigidBody>();
        if (rigidBody && rigidBody->GetActive())
        {
            rigidBody->AddShape(this);
            break;
        }

        entity = entity->GetParent();
    }
}

void CollisionShape::Deactivated()
{
    if (mRigidBody)
    {
        mRigidBody->RemoveShape(this);
    }
}

void CollisionShape::Transformed()
{
    /* Changing the scale requires recreating the shape, don't do this unless
     * necessary. */
    if (GetWorldScale() != mCurrentWorldScale)
    {
        mCurrentWorldScale = GetWorldScale();
        UpdateShape();
    }
    else if (mRigidBody)
    {
        mRigidBody->TransformShape(this);
    }
}

void CollisionShape::SetShape(btCollisionShape* const shape)
{
    shape->setUserPointer(this);

    if (mRigidBody)
    {
        mRigidBody->UpdateShape(this, shape);
    }

    mShape.reset(shape);
}

CollisionShape* CollisionShape::FromBtShape(btCollisionShape* const shape)
{
    return reinterpret_cast<CollisionShape*>(shape->getUserPointer());
}

BoxCollisionShape::BoxCollisionShape() :
    mHalfExtents (0.5f, 0.5f, 0.5f)
{
}

BoxCollisionShape::~BoxCollisionShape()
{
}

void BoxCollisionShape::SetHalfExtents(const glm::vec3& halfExtents)
{
    mHalfExtents = halfExtents;
    UpdateShape();
}

void BoxCollisionShape::UpdateShape()
{
    const glm::vec3 halfExtents = mHalfExtents * GetWorldScale();

    btBoxShape* const shape = new btBoxShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    SetShape(shape);
}

CapsuleCollisionShape::CapsuleCollisionShape() :
    mRadius     (0.5f),
    mHalfHeight (0.5f)
{
}

CapsuleCollisionShape::~CapsuleCollisionShape()
{
}

void CapsuleCollisionShape::SetRadius(const float radius)
{
    mRadius = radius;
    UpdateShape();
}

void CapsuleCollisionShape::SetHalfHeight(const float halfHeight)
{
    mHalfHeight = halfHeight;
    UpdateShape();
}

void CapsuleCollisionShape::UpdateShape()
{
    const glm::vec3 scale = GetWorldScale();

    AssertMsg(scale.x == scale.y && scale.y == scale.z,
              "CapsuleCollisionShape does not support a non-uniform scale");

    btCapsuleShape* const shape = new btCapsuleShape(mRadius * scale.x, mHalfHeight * 2.0f * scale.x);
    SetShape(shape);
}

SphereCollisionShape::SphereCollisionShape() :
    mRadius (0.5f)
{
}

SphereCollisionShape::~SphereCollisionShape()
{
}

void SphereCollisionShape::SetRadius(const float radius)
{
    mRadius = radius;
    UpdateShape();
}

void SphereCollisionShape::UpdateShape()
{
    const glm::vec3 scale = GetWorldScale();

    AssertMsg(scale.x == scale.y && scale.y == scale.z,
              "SphereCollisionShape does not support a non-uniform scale");

    btSphereShape* const shape = new btSphereShape(mRadius * scale.x);
    SetShape(shape);
}
