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

#include "Entity/Component.h"

class btBoxShape;
class btSphereShape;
class btCapsuleShape;
class btCollisionShape;

class RigidBody;

/**
 * This class defines the shape of an object for physics collision detection
 * purposes. This is a base class for real collision shape types.
 *
 * For an Entity to be fully affected by the physics simulation, it must have
 * a RigidBody attached and at least one CollisionShape attached to it or below
 * it.
 */
class CollisionShape : public Component
{
    CLASS();

protected:
                            CollisionShape();
                            ~CollisionShape();

    void                    Activated() override;
    void                    Deactivated() override;
    void                    Transformed() override;

    void                    SetShape(btCollisionShape* const shape);

    /** Update the Bullet shape, called if dimensions changes. */
    virtual void            UpdateShape() = 0;

    static CollisionShape*  FromBtShape(btCollisionShape* const shape);

private:
    UPtr<btCollisionShape>  mShape;
    glm::vec3               mCurrentWorldScale;

    /**
     * RigidBody controlling this shape. This does not always belong to the
     * same entity that the shape belongs to. A RigidBody combines all
     * CollisionShapes on its Entity and its children so this points to the
     * body which this shape is a part of. This field is maintained by
     * RigidBody.
     */
    RigidBody*              mRigidBody;

    friend class RigidBody;
};

/**
 * Box collision shape. A box is defined by its half extents, i.e. half of its
 * width, height and depth. The box extends out by those dimensions in both the
 * positive and negative directions on each axis from the entity's local
 * origin.
 */
class BoxCollisionShape : public CollisionShape
{
    CLASS();

public:
                            BoxCollisionShape();

    /** Half extents of the box. */
    VPROPERTY(glm::vec3, halfExtents);
    const glm::vec3&        GetHalfExtents() const { return mHalfExtents; }
    void                    SetHalfExtents(const glm::vec3& halfExtents);

protected:
                            ~BoxCollisionShape();

    void                    UpdateShape() override;

private:
    glm::vec3               mHalfExtents;

};

/**
 * Capsule collision shape. A capsule is a combination of a cylindrical body
 * and a hemispherical top and bottom. It is defined by the half height of the
 * cylinder, i.e the distance from the entity's local origin to each end of the
 * cylinder, and the radius of the hemispherical ends. Note that with an
 * identity orientation, the capsule is aligned along the Y axis.
 *
 * This component does not support a non-uniform scale, attempting to set one
 * will result in an error.
 */
class CapsuleCollisionShape : public CollisionShape
{
    CLASS();

public:
                            CapsuleCollisionShape();

    /** Radius of the hemispherical parts of the capsule. */
    VPROPERTY(float, radius);
    float                   GetRadius() const       { return mRadius; }
    void                    SetRadius(const float radius);

    /** Half height of the cylindrical part of the capsule. */
    VPROPERTY(float, halfHeight);
    float                   GetHalfHeight() const   { return mHalfHeight; }
    void                    SetHalfHeight(const float halfHeight);

protected:
                            ~CapsuleCollisionShape();

    void                    UpdateShape() override;

private:
    float                   mRadius;
    float                   mHalfHeight;

};

/**
 * Sphere collision shape. A sphere is defined just by its radius, the distance
 * from the entity's local origin to the edge of the sphere.
 *
 * This component does not support a non-uniform scale, attempting to set one
 * will result in an error.
 */
class SphereCollisionShape : public CollisionShape
{
    CLASS();

public:
                            SphereCollisionShape();

    /** Radius of the sphere. */
    VPROPERTY(float, radius);
    float                   GetRadius() const { return mRadius; }
    void                    SetRadius(const float radius);

protected:
                            ~SphereCollisionShape();

    void                    UpdateShape() override;

private:
    float                   mRadius;

};
