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

#include "Physics/PhysicsMaterial.h"

class btCollisionShape;
class btCompoundShape;
class btRigidBody;
class btTransform;

class CollisionShape;

/**
 * The rigid body component is used to add an entity to the physics simulation.
 * Rigid bodies must have a shape defined using the CollisionShape component.
 * The body will not truly become active until it also has an active
 * CollisionShape component available.
 *
 * The overall body shape can be defined as a compound of multiple shapes. This
 * is done by creating child entities and attaching CollisionShapes to them.
 * A RigidBody will make use of all CollisionShapes on its own entity and its
 * descendents.
 *
 * An entity cannot have a RigidBody attached if one is already attached above
 * it in the entity tree.
 */
class RigidBody final : public Component
{
    CLASS();

public:
                            RigidBody();

    /**
     * Static properties.
     */

    /**
     * Mass of the body. If this is set to 0, then the body will become a
     * static body, i.e. it will not be affected by gravity, but it will
     * collide with other bodies.
     */
    VPROPERTY(float, mass);
    float                   GetMass() const             { return mMass; }
    void                    SetMass(const float mass);

    /** Whether the body is static. */
    bool                    IsStatic() const            { return mMass == 0.0f; }

    /** Linear damping factor. */
    VPROPERTY(float, linearDamping);
    float                   GetLinearDamping() const    { return mLinearDamping; }
    void                    SetLinearDamping(const float damping);

    /** Angular damping factor. */
    VPROPERTY(float, angularDamping);
    float                   GetAngularDamping() const    { return mAngularDamping; }
    void                    SetAngularDamping(const float damping);

    /** Physics material used by the body. */
    VPROPERTY(PhysicsMaterialPtr, material);
    PhysicsMaterial*        GetMaterial() const         { return mMaterial; }
    void                    SetMaterial(PhysicsMaterial* const material);

    /**
     * Dynamic properties updated by the simulation. These can only be used
     * when the body is active.
     */

    /**
     * Current linear velocity of the body. Do not set this regularly as it
     * will result in unrealistic behaviour.
     */
    glm::vec3               GetLinearVelocity();
    void                    SetLinearVelocity(const glm::vec3& velocity);

    /**
     * Current angular velocity of the body. Do not set this regularly as it
     * will result in unrealistic behaviour.
     */
    glm::vec3               GetAngularVelocity();
    void                    SetAngularVelocity(const glm::vec3& velocity);

protected:
                            ~RigidBody();

    void                    Activated() override;
    void                    Deactivated() override;
    void                    Transformed() override;

private:
    class MotionState;

private:
    void                    AddShapes(Entity* const entity);
    void                    RemoveShapes(Entity* const entity);

    /**
     * Returns the compound shape if it is being used, otherwise the single
     * attached shape.
     */
    btCollisionShape*       GetShape() const;

    btTransform             CalculateLocalTransform(const CollisionShape* const shape) const;

    /** Callbacks from CollisionShape. */
    void                    AddShape(CollisionShape* const shape);
    void                    RemoveShape(CollisionShape* const shape);
    void                    UpdateShape(CollisionShape* const   shape,
                                        btCollisionShape* const btShape);
    void                    TransformShape(CollisionShape* const shape);

private:
    float                   mMass;
    float                   mLinearDamping;
    float                   mAngularDamping;
    PhysicsMaterialPtr      mMaterial;

    /** Whether a transformation callback from Bullet is in progress. */
    bool                    mUpdatingTransform;

    /** Bullet rigid body. */
    btRigidBody*            mRigidBody;

    /**
     * When this body has more than one collision shape, they are compiled into
     * a compound shape.
     */
    btCompoundShape*        mCompoundShape;

    /** Motion state for receiving motion updates from Bullet. */
    UPtr<MotionState>       mMotionState;

    friend class CollisionShape;
};
