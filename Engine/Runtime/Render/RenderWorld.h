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

#include "Render/RenderEntity.h"
#include "Render/RenderLight.h"
#include "Render/RenderView.h"

struct CullResults
{
    std::vector<const RenderEntity*>    entities;
    std::vector<const RenderLight*>     lights;
};

/**
 * Representation of the world from the renderer's point of view, to be used to
 * search for visible entities and lights when rendering.
 *
 * The World/Entity classes maintain a hierarchical representation of the
 * world's entities, but this is not necessarily a good representation for
 * determining what's visible from a view.
 *
 * Currently, the implementation of this is dumb and just maintains lists of
 * objects that we iterate over and test against the view, but it is intended
 * that in future it will be replaced with something like an octree.
 */
class RenderWorld
{
public:
                                        RenderWorld();
                                        ~RenderWorld();

    void                                AddEntity(RenderEntity* const entity);
    void                                RemoveEntity(RenderEntity* const entity);

    void                                AddLight(RenderLight* const light);
    void                                RemoveLight(RenderLight* const light);

    void                                Cull(const RenderView& view,
                                             CullResults&      outResults) const;

private:
    using RenderEntityList            = IntrusiveList<RenderEntity, &RenderEntity::mWorldListNode>;
    using RenderLightList             = IntrusiveList<RenderLight, &RenderLight::mWorldListNode>;

private:
    RenderEntityList                    mEntities;
    RenderLightList                     mLights;

};
