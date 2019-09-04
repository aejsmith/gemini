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

#include "Render/RenderWorld.h"

#include "Core/Math/Intersect.h"

RenderWorld::RenderWorld()
{
}

RenderWorld::~RenderWorld()
{
    Assert(mEntities.IsEmpty());
}

void RenderWorld::AddEntity(RenderEntity* const inEntity)
{
    mEntities.Append(inEntity);
}

void RenderWorld::RemoveEntity(RenderEntity* const inEntity)
{
    mEntities.Remove(inEntity);
}

void RenderWorld::Cull(const RenderView& inView,
                       CullResults&      outResults) const
{
    const Frustum& frustum = inView.GetFrustum();

    for (const RenderEntity* entity : mEntities)
    {
        if (Math::Intersect(frustum, entity->GetWorldBoundingBox()))
        {
            // TODO: Allocation overhead, this reallocates every insertion.
            outResults.entities.emplace_back(entity);
        }
    }
}