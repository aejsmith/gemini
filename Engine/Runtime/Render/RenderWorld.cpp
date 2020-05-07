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

#include "Render/RenderWorld.h"

#include "Core/Math/Intersect.h"

RenderWorld::RenderWorld()
{
}

RenderWorld::~RenderWorld()
{
    Assert(mEntities.IsEmpty());
}

void RenderWorld::AddEntity(RenderEntity* const entity)
{
    mEntities.Append(entity);
}

void RenderWorld::RemoveEntity(RenderEntity* const entity)
{
    mEntities.Remove(entity);
}

void RenderWorld::AddLight(RenderLight* const light)
{
    mLights.Append(light);
}

void RenderWorld::RemoveLight(RenderLight* const light)
{
    mLights.Remove(light);
}

void RenderWorld::Cull(const RenderView& view,
                       CullResults&      outResults) const
{
    RENDER_PROFILER_FUNC_SCOPE();

    const Frustum& frustum = view.GetFrustum();

    for (const RenderEntity* entity : mEntities)
    {
        if (Math::Intersect(frustum, entity->GetWorldBoundingBox()))
        {
            // TODO: Allocation overhead, this reallocates every insertion.
            outResults.entities.emplace_back(entity);
        }
    }

    for (const RenderLight* light : mLights)
    {
        if (light->Cull(frustum))
        {
            // TODO: Allocation overhead, this reallocates every insertion.
            outResults.lights.emplace_back(light);
        }
    }
}
