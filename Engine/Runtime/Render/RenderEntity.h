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

#include "Core/IntrusiveList.h"
#include "Core/Math/BoundingBox.h"
#include "Core/Math/Transform.h"

#include "Render/RenderDefs.h"

class Renderer;

/**
 * This class is the base for a renderable entity in the world. Renderer
 * components attached to world entities (Entity) add one or more renderable
 * entities (RenderEntity) to the RenderWorld.  There is not necessarily a 1:1
 * mapping between world entities and renderable entities, for example a
 * MeshRenderer has a RenderEntity per-sub-mesh.
 */
class RenderEntity
{
public:
    virtual                         ~RenderEntity();

    const Renderer&                 GetRenderer() const         { return mRenderer; }

    void                            SetTransform(const Transform& inTransform);
    const Transform&                GetTransform() const        { return mTransform; }

    const BoundingBox&              GetWorldBoundingBox() const { return mWorldBoundingBox; }

protected:
                                    RenderEntity(const Renderer& inRenderer);

    /**
     * Get the entity-local bounding box, will be transformed by the entity
     * transform to produce a world bounding box.
     */
    virtual BoundingBox             GetLocalBoundingBox() = 0;

private:
    const Renderer&                 mRenderer;
    Transform                       mTransform;
    BoundingBox                     mWorldBoundingBox;

public:
    IntrusiveListNode               mWorldListNode;

};
