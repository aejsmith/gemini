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

#include "Entity/World.h"

#include "Engine/Serialiser.h"

#include "Entity/Entity.h"
#include "Entity/WorldEditorWindow.h"

#include "Render/RenderWorld.h"

static constexpr char kRootEntityName[] = "Root";

World::World() :
    mRenderWorld    (new RenderWorld),
    mEditorWindow   (new WorldEditorWindow(this))
{
    mRoot         = new Entity();
    mRoot->mName  = kRootEntityName;
    mRoot->mWorld = this;

    mRoot->SetActive(true);
}

World::~World()
{
    mRoot->Destroy();

    delete mRenderWorld;
}

void World::Serialise(Serialiser& serialiser) const
{
    Asset::Serialise(serialiser);

    serialiser.Write("root", mRoot);
}

void World::Deserialise(Serialiser& serialiser)
{
    Asset::Deserialise(serialiser);

    /* Deserialise all entities. */
    EntityPtr newRoot;
    if (serialiser.Read("root", newRoot))
    {
        /* Destroy existing root. */
        mRoot->Destroy();

        mRoot = std::move(newRoot);

        /* Entity::Deserialise() does not activate the root entity, or
         * deserialise its properties (see there for explanation). Do this
         * now, which will activate the whole new world. */
        mRoot->mName = kRootEntityName;

        mRoot->SetActive(true);
    }
}

Entity* World::CreateEntity(std::string name)
{
    return mRoot->CreateChild(std::move(name));
}

void World::Tick(const float delta)
{
    mRoot->Tick(delta);
}
