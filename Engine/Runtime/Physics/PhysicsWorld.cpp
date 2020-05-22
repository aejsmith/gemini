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

#include "Physics/PhysicsWorld.h"

#include "Engine/AssetManager.h"

#include "Physics/PhysicsInternal.h"
#include "Physics/PhysicsMaterial.h"

static constexpr char kDefaultMaterialPath[] = "Engine/PhysicsMaterials/Default";

PhysicsWorld::PhysicsWorld()
{
    mCollisionConfiguration.reset(new btDefaultCollisionConfiguration());
    mDispatcher.reset(new btCollisionDispatcher(mCollisionConfiguration.get()));
    mBroadphase.reset(new btDbvtBroadphase());
    mConstraintSolver.reset(new btSequentialImpulseConstraintSolver());
    mWorld.reset(new btDiscreteDynamicsWorld(mDispatcher.get(),
                                             mBroadphase.get(),
                                             mConstraintSolver.get(),
                                             mCollisionConfiguration.get()));

    SetGravity(glm::vec3(0.0f, -9.81f, 0.0f));

    /* See GetDefaultMaterial(). */
    mDefaultMaterial = AssetManager::Get().Load<PhysicsMaterial>(kDefaultMaterialPath);
}

PhysicsWorld::~PhysicsWorld()
{
}

void PhysicsWorld::SetGravity(const glm::vec3& gravity)
{
    mGravity = gravity;
    mWorld->setGravity(BulletUtil::ToBullet(gravity));
}

void PhysicsWorld::Tick(const float delta)
{
    PHYSICS_PROFILER_FUNC_SCOPE();

    /* We run physics fixed at 60Hz, with interpolated motion between timesteps
     * when the framerate is variable. */
    mWorld->stepSimulation(delta, 10, 1.0f / 60.0f);
}

PhysicsMaterialPtr PhysicsWorld::GetDefaultMaterial()
{
    /*
     * This is not too nice. RigidBody's constructor uses this. However, when
     * that is called, it is not yet associated with a world, and during
     * deserialisation it will not be able to fetch the world being loaded from
     * Engine yet either.
     *
     * Therefore, we do a load every call to this. However, in the constructor
     * we load it and keep a reference to it while the world is alive. This
     * means every time this is called, we should match the already loaded
     * asset.
     */
    return AssetManager::Get().Load<PhysicsMaterial>(kDefaultMaterialPath);
}
