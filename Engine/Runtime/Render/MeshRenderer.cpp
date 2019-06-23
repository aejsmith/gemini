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

#include "Render/MeshRenderer.h"

#include "Render/RenderEntity.h"

class SubMeshRenderEntity final : public RenderEntity
{
public:
                                SubMeshRenderEntity(const MeshRenderer& inRenderer,
                                                    const Mesh&         inMesh,
                                                    const SubMesh&      inSubMesh);

protected:
    BoundingBox                 GetLocalBoundingBox() override;

private:
    const MeshRenderer&         GetMeshRenderer() const
                                    { return static_cast<const MeshRenderer&>(GetRenderer()); }

private:
    const Mesh&                 mMesh;
    const SubMesh&              mSubMesh;

};

SubMeshRenderEntity::SubMeshRenderEntity(const MeshRenderer& inRenderer,
                                         const Mesh&         inMesh,
                                         const SubMesh&      inSubMesh) :
    RenderEntity    (inRenderer),
    mMesh           (inMesh),
    mSubMesh        (inSubMesh)
{
}

BoundingBox SubMeshRenderEntity::GetLocalBoundingBox()
{
    return mSubMesh.GetBoundingBox();
}

MeshRenderer::MeshRenderer()
{
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::SetMesh(Mesh* const inMesh)
{
    /* Deactivate and reactivate to re-create the RenderEntities. */
    const bool wasActive = GetActive();
    SetActive(false);

    mMesh = inMesh;

    SetActive(wasActive);
}

Renderer::RenderEntityArray MeshRenderer::CreateRenderEntities()
{
    AssertMsg(mMesh,
              "No mesh set for MeshRenderer on '%s'",
              GetEntity()->GetName().c_str());

    RenderEntityArray renderEntities(mMesh->GetSubMeshCount());

    for (size_t i = 0; i < mMesh->GetSubMeshCount(); i++)
    {
        renderEntities[i] = new SubMeshRenderEntity(*this,
                                                    *mMesh,
                                                    mMesh->GetSubMesh(i));
    }

    return renderEntities;
}
