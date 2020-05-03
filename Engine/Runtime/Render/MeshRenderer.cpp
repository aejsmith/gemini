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

#include "Engine/AssetManager.h"
#include "Engine/DebugWindow.h"
#include "Engine/Serialiser.h"

#include "Render/EntityDrawList.h"
#include "Render/RenderEntity.h"

class SubMeshRenderEntity final : public RenderEntity
{
public:
                                SubMeshRenderEntity(const MeshRenderer& renderer,
                                                    const Mesh&         mesh,
                                                    const SubMesh&      subMesh,
                                                    Material&           material);

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

SubMeshRenderEntity::SubMeshRenderEntity(const MeshRenderer& renderer,
                                         const Mesh&         mesh,
                                         const SubMesh&      subMesh,
                                         Material&           material) :
    RenderEntity    (renderer, material),
    mMesh           (mesh),
    mSubMesh        (subMesh)
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
    return mSubMesh.GetTopology();
}

void SubMeshRenderEntity::GetGeometry(EntityDrawCall& ioDrawCall) const
{
    const GPUVertexBufferBitset usedVertexBuffers = mMesh.GetUsedVertexBuffers();

    for (size_t i = 0; i < kMaxVertexAttributes; i++)
    {
        if (usedVertexBuffers.Test(i))
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

void MeshRenderer::Serialise(Serialiser& serialiser) const
{
    EntityRenderer::Serialise(serialiser);

    serialiser.BeginGroup("materials");

    for (size_t i = 0; i < mMesh->GetMaterialCount(); i++)
    {
        if (mMaterials[i])
        {
            serialiser.Write(mMesh->GetMaterialName(i).c_str(), mMaterials[i]);
        }
    }

    serialiser.EndGroup();
}

void MeshRenderer::Deserialise(Serialiser& serialiser)
{
    EntityRenderer::Deserialise(serialiser);

    bool success = true;
    Unused(success);

    success &= serialiser.BeginGroup("materials");
    Assert(success);

    for (size_t i = 0; i < mMesh->GetMaterialCount(); i++)
    {
        success &= serialiser.Read(mMesh->GetMaterialName(i).c_str(), mMaterials[i]);
        Assert(success);
    }

    serialiser.EndGroup();
}

void MeshRenderer::SetMesh(Mesh* const mesh)
{
    /* Need to recreate the RenderEntities to take effect. */
    ScopedComponentDeactivation deactivate(this);

    mMesh = mesh;
    mMaterials.resize(mesh->GetMaterialCount());

    /* TODO: Can't reactivate unless there is a material in all slots. Should
     * perhaps populate new slots with a dummy material. */
}

EntityRenderer::RenderEntityArray MeshRenderer::CreateRenderEntities()
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

Material* MeshRenderer::GetMaterial(const std::string& name) const
{
    size_t index;
    const bool found = mMesh->GetMaterial(name, index);
    Assert(found);

    return (found) ? mMaterials[index] : nullptr;
}

void MeshRenderer::SetMaterial(const uint32_t  index,
                               Material* const material)
{
    Assert(index < mMaterials.size());

    /* Need to recreate the RenderEntities to take effect. */
    ScopedComponentDeactivation deactivate(this);
    mMaterials[index] = material;
}

void MeshRenderer::SetMaterial(const std::string& name,
                               Material* const    material)
{
    size_t index;
    const bool found = mMesh->GetMaterial(name, index);
    Assert(found);

    if (found)
    {
        SetMaterial(index, material);
    }
}

void MeshRenderer::CustomDebugUIEditor(const uint32_t        flags,
                                       std::vector<Object*>& ioChildren)
{
    if (!mMesh)
    {
        return;
    }

    ImGui::AlignTextToFramePadding(); ImGui::Text("materials");
    ImGui::NextColumn(); ImGui::NextColumn();

    for (size_t i = 0; i < mMesh->GetMaterialCount(); i++)
    {
        ImGui::PushID(&mMaterials[i]);

        ImGui::Indent();
        ImGui::AlignTextToFramePadding();
        ImGui::Text(mMesh->GetMaterialName(i).c_str());
        ImGui::Unindent();

        ImGui::NextColumn();

        const bool activate = ImGui::Button("Select");

        AssetPtr material = mMaterials[i];

        ImGui::SameLine();
        ImGui::Text("%s", (material) ? material->GetPath().c_str() : "null");

        if (AssetManager::Get().DebugUIAssetSelector(material, Material::staticMetaClass, activate))
        {
            SetMaterial(i, static_cast<Material*>(material.Get()));
        }

        ImGui::NextColumn();

        ImGui::PopID();
    }
}
