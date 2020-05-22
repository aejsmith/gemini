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

#include "Physics/RigidBody.h"

#include "Entity/World.h"

#include "Physics/CollisionShape.h"
#include "Physics/PhysicsInternal.h"
#include "Physics/PhysicsWorld.h"

/** Synchronises Bullet and internal states. */
class RigidBody::MotionState : public btMotionState
{
public:
                            explicit MotionState(RigidBody* const rigidBody);

    void                    getWorldTransform(btTransform& transform) const override;
    void                    setWorldTransform(const btTransform& transform) override;

private:
    RigidBody*              mRigidBody;

};

RigidBody::MotionState::MotionState(RigidBody* const rigidBody) :
    mRigidBody (rigidBody)
{
}

void RigidBody::MotionState::getWorldTransform(btTransform& transform) const
{
    transform.setRotation(BulletUtil::ToBullet(mRigidBody->GetWorldOrientation()));
    transform.setOrigin(BulletUtil::ToBullet(mRigidBody->GetWorldPosition()));
}

void RigidBody::MotionState::setWorldTransform(const btTransform &transform)
{
    /* This prevents us from trying to update Bullet state in response to
     * Bullet updating our state. */
    mRigidBody->mUpdatingTransform = true;

    mRigidBody->GetEntity()->SetTransform(BulletUtil::FromBullet(transform.getOrigin()),
                                          BulletUtil::FromBullet(transform.getRotation()),
                                          mRigidBody->GetScale());

    mRigidBody->mUpdatingTransform = false;
}

RigidBody::RigidBody() :
    mMass              (0.0f),
    mLinearDamping     (0.0f),
    mAngularDamping    (0.0f),
    mMaterial          (PhysicsWorld::GetDefaultMaterial()),
    mUpdatingTransform (false),
    mRigidBody         (nullptr),
    mCompoundShape     (nullptr),
    mMotionState       (new MotionState(this))
{
}

RigidBody::~RigidBody()
{
    Assert(!mRigidBody);
    Assert(!mCompoundShape);
}

void RigidBody::SetMass(const float mass)
{
    mMass = mass;

    if (mRigidBody)
    {
        btCollisionShape* const shape = GetShape();

        btVector3 inertia(0.0f, 0.0f, 0.0f);
        shape->calculateLocalInertia(mMass, inertia);

        mRigidBody->setMassProps(mMass, inertia);
    }
}

void RigidBody::SetLinearDamping(const float damping)
{
    mLinearDamping = damping;

    if (mRigidBody)
    {
        mRigidBody->setDamping(mLinearDamping, mAngularDamping);
    }
}

void RigidBody::SetAngularDamping(const float damping)
{
    mAngularDamping = damping;

    if (mRigidBody)
    {
        mRigidBody->setDamping(mLinearDamping, mAngularDamping);
    }
}

void RigidBody::SetMaterial(PhysicsMaterial* const material)
{
    Assert(material);

    mMaterial = material;

    if (mRigidBody)
    {
        mRigidBody->setRestitution(mMaterial->GetRestitution());
        mRigidBody->setFriction(mMaterial->GetFriction());
    }
}

glm::vec3 RigidBody::GetLinearVelocity()
{
    Assert(mRigidBody);
    return BulletUtil::FromBullet(mRigidBody->getLinearVelocity());
}

void RigidBody::SetLinearVelocity(const glm::vec3& velocity)
{
    Assert(mRigidBody);
    mRigidBody->setLinearVelocity(BulletUtil::ToBullet(velocity));
}

glm::vec3 RigidBody::GetAngularVelocity()
{
    Assert(mRigidBody);
    return BulletUtil::FromBullet(mRigidBody->getAngularVelocity());
}

void RigidBody::SetAngularVelocity(const glm::vec3& velocity)
{
    Assert(mRigidBody);
    mRigidBody->setAngularVelocity(BulletUtil::ToBullet(velocity));
}

void RigidBody::AddShapes(Entity* const entity)
{
    /* Depending on component activation order the shape could already have
     * added itself. */
    CollisionShape* const shape = entity->FindComponent<CollisionShape>();
    if (shape && shape->GetActiveInWorld() && !shape->mRigidBody)
    {
        AddShape(shape);
    }

    entity->VisitActiveChildren([this] (Entity* const entity) { AddShapes(entity); });
}

void RigidBody::Activated()
{
    #if GEMINI_BUILD_DEBUG
        /* Ensure no other RigidBody components are above us. */
        for (Entity* parent = GetEntity()->GetParent(); parent; parent = parent->GetParent())
        {
            RigidBody* const other = parent->FindComponent<RigidBody>();
            AssertMsg(!other, "A RigidBody as child of another is not allowed");
        }
    #endif

    /*
     * Scan down for active CollisionShapes that we should take control of.
     * Note that the body will only truly become active once it also has at
     * least one shape, so creation of the body is deferred until AddShape().
     */
    AddShapes(GetEntity());
}

void RigidBody::RemoveShapes(Entity* const entity)
{
    /* Depending on component deactivation order the shape could already have
     * removed itself. */
    CollisionShape* const shape = entity->FindComponent<CollisionShape>();
    if (shape && shape->GetActiveInWorld() && shape->mRigidBody == this)
    {
        RemoveShape(shape);
    }

    entity->VisitActiveChildren([this] (Entity* const entity) { RemoveShapes(entity); });
}

void RigidBody::Deactivated()
{
    /* Scan down for active CollisionShapes that we should release control of. */
    RemoveShapes(GetEntity());

    /* Should be destroyed by the removal of shapes. */
    Assert(!mRigidBody);
}

void RigidBody::Transformed()
{
    if (mRigidBody && !mUpdatingTransform)
    {
        const btTransform transform(BulletUtil::ToBullet(GetWorldOrientation()),
                                    BulletUtil::ToBullet(GetWorldPosition()));

        mRigidBody->setWorldTransform(transform);

        /*
         * We're forcing a change of the transformation, so update Bullet's
         * interpolation transformation as well. Without doing this the body
         * may flick back to its old position and then interpolate to the new
         * one.
         */
        mRigidBody->setInterpolationWorldTransform(transform);
    }
}

btCollisionShape* RigidBody::GetShape() const
{
    return (mCompoundShape) ? mCompoundShape : mRigidBody->getCollisionShape();
}

btTransform RigidBody::CalculateLocalTransform(const CollisionShape* const shape) const
{
    const glm::vec3 position    = shape->GetWorldPosition() - GetWorldPosition();
    const glm::quat orientation = Math::QuatDifference(GetWorldOrientation(), shape->GetWorldOrientation());

    return btTransform(BulletUtil::ToBullet(orientation), BulletUtil::ToBullet(position));
}

void RigidBody::AddShape(CollisionShape* const shape)
{
    Assert(GetActiveInWorld());
    Assert(!shape->mRigidBody);

    shape->mRigidBody = this;

    /*
     * If we don't have a compound shape yet and this is not the first shape,
     * or the shape being added is attached to a child entity (and therefore
     * needs a transformation relative to the body), we must create a compound
     * shape.
     */
    if (!mCompoundShape && (mRigidBody || shape->GetEntity() != GetEntity()))
    {
        mCompoundShape = new btCompoundShape();

        if (mRigidBody)
        {
            /*
             * Move the existing shape over to the compound. Since it was
             * attached directly it must exist on the same entity as the body
             * and therefore has an identity transformation.
             */
            CollisionShape* const currShape = CollisionShape::FromBtShape(mRigidBody->getCollisionShape());
            mCompoundShape->addChildShape(btTransform::getIdentity(), currShape->mShape.get());

            mRigidBody->setCollisionShape(mCompoundShape);
        }
    }

    /* If we have a compound shape, add the shape to it. */
    if (mCompoundShape)
    {
        const btTransform localTransform = CalculateLocalTransform(shape);
        mCompoundShape->addChildShape(localTransform, shape->mShape.get());
    }

    /* Create the body if we don't have one yet. */
    if (!mRigidBody)
    {
        btCollisionShape* const bodyShape = (mCompoundShape) ? mCompoundShape : shape->mShape.get();

        btVector3 inertia(0.0f, 0.0f, 0.0f);
        bodyShape->calculateLocalInertia(mMass, inertia);

        btRigidBody::btRigidBodyConstructionInfo constructionInfo(
            mMass,
            mMotionState.get(),
            bodyShape,
            inertia);
        constructionInfo.m_linearDamping  = mLinearDamping;
        constructionInfo.m_angularDamping = mAngularDamping;
        constructionInfo.m_friction       = mMaterial->GetFriction();
        constructionInfo.m_restitution    = mMaterial->GetRestitution();

        mRigidBody = new btRigidBody(constructionInfo);

        GetWorld()->GetPhysicsWorld()->GetBtWorld()->addRigidBody(mRigidBody);
    }
}

void RigidBody::RemoveShape(CollisionShape* const shape)
{
    Assert(mRigidBody);
    Assert(shape->mRigidBody == this);

    bool destroyBody;

    if (mCompoundShape)
    {
        /* Remove the shape from the compound. */
        mCompoundShape->removeChildShape(shape->mShape.get());
        destroyBody = mCompoundShape->getNumChildShapes() == 0;
    }
    else
    {
        /* Only shape attached to the body. */
        Assert(mRigidBody->getCollisionShape() == shape->mShape.get());
        destroyBody = true;
    }

    if (destroyBody)
    {
        GetWorld()->GetPhysicsWorld()->GetBtWorld()->removeRigidBody(mRigidBody);

        delete mRigidBody;
        mRigidBody = nullptr;

        if (mCompoundShape)
        {
            delete mCompoundShape;
            mCompoundShape = nullptr;
        }
    }

    shape->mRigidBody = nullptr;
}

void RigidBody::UpdateShape(CollisionShape* const   shape,
                            btCollisionShape* const btShape)
{
    btCollisionShape* const old = shape->mShape.get();

    if (mCompoundShape)
    {
        mCompoundShape->removeChildShape(old);

        const btTransform localTransform = CalculateLocalTransform(shape);
        mCompoundShape->addChildShape(localTransform, btShape);
    }
    else
    {
        Assert(mRigidBody->getCollisionShape() == old);
        mRigidBody->setCollisionShape(btShape);
    }
}

void RigidBody::TransformShape(CollisionShape* const shape)
{
    /* Don't need to do anything if the shape is attached to same entity, the
     * Transformed() callback will handle it. */
    if (!mUpdatingTransform && shape->GetEntity() != GetEntity())
    {
        Assert(mCompoundShape);

        btCollisionShape* const btShape = shape->mShape.get();

        for (int i = 0; i < mCompoundShape->getNumChildShapes(); i++)
        {
            if (mCompoundShape->getChildShape(i) == btShape)
            {
                const btTransform localTransform = CalculateLocalTransform(shape);
                mCompoundShape->updateChildTransform(i, localTransform);
                break;
            }
        }
    }
}
