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

#pragma once

#include "Engine/Asset.h"

class Entity;
class RenderWorld;
class WorldEditorWindow;

/**
 * This class holds the entire game world. It holds a hierarchical view of all
 * entities in the world. Other systems (e.g. the renderer and the physics
 * system) hold their own views of the world in addition to this. Adding
 * entities to these systems is handled automatically when they are activated
 * in the world.
 */
class World final : public Asset
{
    CLASS();

public:
    Entity*                     GetRoot()           { return mRoot; }
    const Entity*               GetRoot() const     { return mRoot; }

    /** Create a new entity at the root of the hierarchy. */
    Entity*                     CreateEntity(std::string name);

    RenderWorld*                GetRenderWorld()    { return mRenderWorld; }

    void                        Tick(const float delta);

protected:
                                World();
                                ~World();

    void                        Serialise(Serialiser& serialiser) const override;
    void                        Deserialise(Serialiser& serialiser) override;

private:
    ObjPtr<Entity>              mRoot;

    RenderWorld* const          mRenderWorld;

    UPtr<WorldEditorWindow>     mEditorWindow;

    friend class Engine;
};

using WorldPtr = ObjPtr<World>;
