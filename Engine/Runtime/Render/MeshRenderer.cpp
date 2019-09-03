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

#include "Render/EntityDrawList.h"
#include "Render/RenderEntity.h"

class SubMeshRenderEntity final : public RenderEntity
{
public:
                                SubMeshRenderEntity(const MeshRenderer& inRenderer,
                                                    const Mesh&         inMesh,
                                                    const SubMesh&      inSubMesh,
                                                    Material&           inMaterial);

protected:
    BoundingBox                 GetLocalBoundingBox() override;

    GPUVertexInputStateRef      GetVertexInputState() const override;
    GPUPrimitiveTopology        GetPrimitiveTopology() const override;

    void                        GetGeometry(EntityDrawCall& ioDrawCall) const override;

private:
    const MeshRenderer&         GetMeshRenderer() const
                                    { return static_cast<const MeshRenderer&>(GetRenderer()); }

private:
    const Mesh&                 mMesh;
    const SubMesh&              mSubMesh;

};

SubMeshRenderEntity::SubMeshRenderEntity(const MeshRenderer& inRenderer,
                                         const Mesh&         inMesh,
                                         const SubMesh&      inSubMesh,
                                         Material&           inMaterial) :
    RenderEntity    (inRenderer, inMaterial),
    mMesh           (inMesh),
    mSubMesh        (inSubMesh)
{
}

BoundingBox SubMeshRenderEntity::GetLocalBoundingBox()
{
    return mSubMesh.GetBoundingBox();
}

GPUVertexInputStateRef SubMeshRenderEntity::GetVertexInputState() const
{
    return mMesh.GetVertexInputState();
}

GPUPrimitiveTopology SubMeshRenderEntity::GetPrimitiveTopology() const
{
    return kGPUPrimitiveTopology_TriangleList;
}

void SubMeshRenderEntity::GetGeometry(EntityDrawCall& ioDrawCall) const
{
    const GPUVertexBufferBitset usedVertexBuffers = mMesh.GetUsedVertexBuffers();

    for (size_t i = 0; i < kMaxVertexAttributes; i++)
    {
        if (usedVertexBuffers.test(i))
        {
            ioDrawCall.vertexBuffers[i].buffer = mMesh.GetVertexBuffer(i);
        }
    }

    if (mSubMesh.IsIndexed())
    {
        ioDrawCall.indexBuffer.buffer = mSubMesh.GetIndexBuffer();
        ioDrawCall.indexType          = mSubMesh.GetIndexType();
    }
    else
    {
        ioDrawCall.vertexOffset = mSubMesh.GetVertexOffset();
    }

    ioDrawCall.vertexCount = mSubMesh.GetCount();
}

MeshRenderer::MeshRenderer()
{
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::SetMesh(Mesh* const inMesh)
{
    /* Need to recreate the RenderEntities to take effect. */
    ScopedComponentDeactivation deactivate(this);

    mMesh = inMesh;
    mMaterials.resize(inMesh->GetMaterialCount());

    /* TODO: Can't reactivate unless there is a material in all slots. Should
     * perhaps populate new slots with a dummy material. */
}

Renderer::RenderEntityArray MeshRenderer::CreateRenderEntities()
{
    AssertMsg(mMesh,
              "No mesh set for MeshRenderer on '%s'",
              GetEntity()->GetName().c_str());

    RenderEntityArray renderEntities(mMesh->GetSubMeshCount());

    for (size_t i = 0; i < mMesh->GetSubMeshCount(); i++)
    {
        const SubMesh& subMesh = mMesh->GetSubMesh(i);

        AssertMsg(mMaterials[subMesh.GetMaterial()],
                  "No material set at index %u for MeshRenderer on '%s'",
                  subMesh.GetMaterial(),
                  GetEntity()->GetName().c_str());

        renderEntities[i] = new SubMeshRenderEntity(*this,
                                                    *mMesh,
                                                    subMesh,
                                                    *mMaterials[subMesh.GetMaterial()]);
    }

    return renderEntities;
}

Material* MeshRenderer::GetMaterial(const std::string& inName) const
{
    uint32_t index;
    const bool found = mMesh->GetMaterial(inName, index);
    Assert(found);

    return (found) ? mMaterials[index] : nullptr;
}

void MeshRenderer::SetMaterial(const uint32_t  inIndex,
                               Material* const inMaterial)
{
    Assert(inIndex < mMaterials.size());

    /* Need to recreate the RenderEntities to take effect. */
    ScopedComponentDeactivation deactivate(this);
    mMaterials[inIndex] = inMaterial;
}

void MeshRenderer::SetMaterial(const std::string& inName,
                               Material* const    inMaterial)
{
    uint32_t index;
    const bool found = mMesh->GetMaterial(inName, index);
    Assert(found);

    if (found)
    {
        SetMaterial(index, inMaterial);
    }
}
