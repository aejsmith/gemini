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

#include "Core/HashTable.h"
#include "Core/Math.h"
#include "Core/String.h"

#include "Engine/AssetLoader.h"

#include <vector>

/** Indices into the vertex element arrays for a single vertex. */
struct OBJVertexKey
{
    uint32_t                    position;
    uint32_t                    texcoord;
    uint32_t                    normal;
};

DEFINE_HASH_MEM_OPS(OBJVertexKey);

struct OBJVertex
{
    glm::vec3                   position;
    glm::vec2                   texcoord;
    glm::vec3                   normal;
};

struct OBJSubMesh
{
    std::vector<uint16_t>       indices;
};

class OBJLoader : public AssetLoader
{
    CLASS();

public:
                                OBJLoader();

    const char*                 GetExtension() const override
                                    { return "obj"; }

protected:
                                ~OBJLoader();

    AssetPtr                    Load() override;

private:
    using SubMeshMap          = HashMap<std::string, OBJSubMesh>;
    using VertexMap           = HashMap<OBJVertexKey, size_t>;

private:
    bool                        Parse();

    template <typename ElementType>
    bool                        AddVertexElement(const std::vector<std::string>& tokens,
                                                 std::vector<ElementType>&       ioArray);

    bool                        AddFace(const std::vector<std::string>& tokens);

    AssetPtr                    BuildMesh();

private:
    size_t                      mCurrentLine;
    std::string                 mCurrentMaterial;
    OBJSubMesh*                 mCurrentSubMesh;

    /** Map of material name to submesh. We use a single submesh per material. */
    SubMeshMap                  mSubMeshMap;

    /** Vertex elements. */
    std::vector<glm::vec3>      mPositions;
    std::vector<glm::vec2>      mTexcoords;
    std::vector<glm::vec3>      mNormals;

    /** Map from OBJVertexKey to a vertex buffer index. */
    VertexMap                   mVertexMap;

    uint32_t                    mVertexCount;
};
