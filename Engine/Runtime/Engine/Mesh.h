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

#pragma once

#include "Core/ByteArray.h"
#include "Core/Math/BoundingBox.h"

#include "Engine/Asset.h"

#include "GPU/GPUState.h"

class GPUBuffer;
class Mesh;

/** Sub-component of a mesh. */
class SubMesh
{
public:
    uint32_t                    GetMaterial() const { return mMaterial; }
    GPUPrimitiveTopology        GetTopology() const { return mTopology; }
    bool                        IsIndexed() const   { return mIndexed; }
    uint32_t                    GetCount() const    { return mCount; }

    uint32_t                    GetVertexOffset() const
                                    { Assert(!mIndexed); return mVertexOffset; }

    GPUIndexType                GetIndexType() const
                                    { Assert(mIndexed); return mIndexType; }

    GPUBuffer*                  GetIndexBuffer() const
                                    { Assert(mIndexed); return mIndexBuffer; }

    const BoundingBox&          GetBoundingBox() const  { return mBoundingBox; }

private:
                                SubMesh(Mesh& parent) : mParent (parent) {}
                                ~SubMesh() {}

private:
    Mesh&                       mParent;
    uint32_t                    mMaterial;
    GPUPrimitiveTopology        mTopology;
    bool                        mIndexed;

    /** Vertex or index count, depending on if indexed. */
    uint32_t                    mCount;

    union
    {
        uint32_t                mVertexOffset;
        GPUIndexType            mIndexType;
    };

    GPUBuffer*                  mIndexBuffer;
    BoundingBox                 mBoundingBox;

    /** CPU-side index data. This is discarded after the mesh is built. */
    ByteArray                   mIndexData;

    friend class Mesh;
};

/**
 * This class stores a 3D mesh. A mesh is comprised of one or more submeshes.
 * Each submesh can be assigned a different material, allowing different parts
 * of a mesh to use different materials.
 *
 * The process of creating a mesh from scratch is as follows:
 *  1. Define a vertex data layout and count.
 *  3. Set vertex data for each buffer defined by the layout.
 *  4. Add material definitions.
 *  5. Add submeshes (specifying a material and index data).
 *  6. Build the mesh.
 *
 * Building creates GPU buffers containing the mesh data for rendering,
 * computes bounding boxes, etc. Currently, once built, a mesh cannot be
 * changed.
 */
class Mesh final : public Asset
{
    CLASS();

public:
                                Mesh();

    size_t                      GetSubMeshCount() const
                                    { return mSubMeshes.size(); }
    const SubMesh&              GetSubMesh(const size_t index) const
                                    { Assert(index < GetSubMeshCount()); return *mSubMeshes[index]; }

    GPUVertexInputStateRef      GetVertexInputState() const
                                    { Assert(mIsBuilt); return mVertexInputState; }
    GPUVertexBufferBitset       GetUsedVertexBuffers() const
                                    { Assert(mIsBuilt); return mUsedVertexBuffers; }
    GPUBuffer*                  GetVertexBuffer(const size_t index) const
                                    { Assert(mIsBuilt); return mVertexBuffers[index]; }

    size_t                      GetMaterialCount() const
                                    { return mMaterials.size(); }
    bool                        GetMaterial(const std::string& name,
                                            size_t&            outIndex) const;
    const std::string&          GetMaterialName(const size_t index) const;

    /**
     * Mesh build methods.
     */

    void                        SetVertexLayout(const GPUVertexInputStateDesc& desc,
                                                const uint32_t                 count);

    /**
     * Set data for a buffer. Expected data size is the stride of the buffer
     * defined in the layout multiplied by the vertex count. The void* version
     * copies the given data.
     */
    void                        SetVertexData(const uint32_t index,
                                              ByteArray      data);
    void                        SetVertexData(const uint32_t    index,
                                              const void* const data);

    /**
     * Add a material slot to the mesh. Material slots are given a name, which
     * allows materials to be set by name on the mesh renderer. The name maps
     * to an index, which is returned by this.
     */
    uint32_t                    AddMaterial(std::string name);

    /**
     * Add a non-indexed submesh which just uses a contiguous range of the
     * mesh's vertex data.
     */
    void                        AddSubMesh(const uint32_t             materialIndex,
                                           const GPUPrimitiveTopology topology,
                                           const uint32_t             vertexOffset,
                                           const uint32_t             vertexCount);

    /**
     * Add a submesh which is rendered using indices into the mesh's vertex
     * data. The void* version copies the given data.
     */
    void                        AddIndexedSubMesh(const uint32_t             materialIndex,
                                                  const GPUPrimitiveTopology topology,
                                                  const uint32_t             indexCount,
                                                  const GPUIndexType         indexType,
                                                  ByteArray                  indexData);
    void                        AddIndexedSubMesh(const uint32_t             materialIndex,
                                                  const GPUPrimitiveTopology topology,
                                                  const uint32_t             indexCount,
                                                  const GPUIndexType         indexType,
                                                  const void* const          indexData);

    /**
     * Build the mesh. After this is called, the mesh cannot be changed.
     * This must currently be called on the main thread (TODO).
     */
    void                        Build();

private:
    /**
     * Array of material names. Using an array rather than a map as typically
     * the number of materials will be small enough that looking up in an
     * array is likely more efficient.
     */
    using MaterialArray       = std::vector<std::string>;

private:
                                ~Mesh();

    void                        Serialise(Serialiser& serialiser) const override;
    void                        Deserialise(Serialiser& serialiser) override;

    void                        PathChanged() override;

    void                        CalculateBoundingBox(SubMesh* const subMesh);

    glm::vec4                   LoadAttribute(const GPUAttributeSemantic semantic,
                                              const uint8_t              semanticIndex,
                                              const SubMesh* const       subMesh,
                                              const uint32_t             index);
    glm::vec4                   LoadAttribute(const GPUAttributeSemantic semantic,
                                              const uint8_t              semanticIndex,
                                              const uint32_t             vertexIndex);

private:
    bool                        mIsBuilt;
    GPUVertexInputStateRef      mVertexInputState;
    GPUVertexBufferBitset       mUsedVertexBuffers;
    uint32_t                    mVertexCount;
    MaterialArray               mMaterials;
    std::vector<SubMesh*>       mSubMeshes;

    GPUBuffer*                  mVertexBuffers[kMaxVertexAttributes];

    /** CPU-side vertex data. This is discarded after the mesh is built. */
    ByteArray                   mVertexData[kMaxVertexAttributes];
};

using MeshPtr = ObjPtr<Mesh>;

inline const std::string& Mesh::GetMaterialName(const size_t index) const
{
    Assert(index < mMaterials.size());
    return mMaterials[index];
}
