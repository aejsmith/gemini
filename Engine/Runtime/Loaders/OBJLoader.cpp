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

#include "Loaders/OBJLoader.h"

#include "Engine/Mesh.h"

/**
 * TODO:
 *  - Can have models without texcoords or normals. Should add functionality
 *    somewhere to dynamically define a vertex layout.
 *  - This is hideously slow. ReadLine() reads individual characters at a time,
 *    and there's a metric fuckton of vector reallocations going on.
 */

OBJLoader::OBJLoader() :
    mCurrentLine        (0),
    mCurrentMaterial    ("default"),
    mCurrentSubMesh     (nullptr),
    mVertexCount        (0)
{
}

OBJLoader::~OBJLoader()
{
}

AssetPtr OBJLoader::Load()
{
    if (!Parse())
    {
        return nullptr;
    }

    return BuildMesh();
}

bool OBJLoader::Parse()
{
    std::string line;
    while (mData->ReadLine(line))
    {
        mCurrentLine++;

        std::vector<std::string> tokens;
        StringUtils::Tokenize(line, tokens, " \r");
        if (tokens.empty())
        {
            continue;
        }

        if (tokens[0] == "v")
        {
            if (!AddVertexElement(tokens, mPositions))
            {
                return false;
            }
        }
        else if (tokens[0] == "vt")
        {
            if (!AddVertexElement(tokens, mTexcoords))
            {
                return false;
            }
        }
        else if (tokens[0] == "vn")
        {
            if (!AddVertexElement(tokens, mNormals))
            {
                return false;
            }
        }
        else if (tokens[0] == "f")
        {
            if (!AddFace(tokens))
            {
                return false;
            }
        }
        else if (tokens[0] == "usemtl")
        {
            if (tokens.size() != 2)
            {
                LogError("%s: %u: Expected single material name", mPath, mCurrentLine);
                return false;
            }

            if (tokens[1] != mCurrentMaterial)
            {
                mCurrentMaterial = tokens[1];
                mCurrentSubMesh  = nullptr;
            }
        }
        else
        {
            /* Ignore unknown lines. Most of them are irrelevant to us. */
        }
    }

    return true;
}

template <typename ElementType>
bool OBJLoader::AddVertexElement(const std::vector<std::string>& tokens,
                                 std::vector<ElementType>&       ioArray)
{
    ElementType value;

    if (tokens.size() < value.length() + 1)
    {
        LogError("%s: %u: Expected %d values",
                 mPath,
                 mCurrentLine,
                 value.length());

        return false;
    }

    for (size_t i = 0; i < value.length(); i++)
    {
        const char* const token = tokens[i + 1].c_str();
        char* end;

        value[i] = strtof(token, &end);
        if (end != token + tokens[i + 1].length())
        {
            LogError("%s: %u: Expected float value", mPath, mCurrentLine);
            return false;
        }
    }

    ioArray.emplace_back(value);
    return true;
}

bool OBJLoader::AddFace(const std::vector<std::string>& tokens)
{
    /* If we don't have a current submesh, we must get a new one. If there is
     * an existing submesh using the same material, we merge into that. */
    if (!mCurrentSubMesh)
    {
        auto ret = mSubMeshMap.emplace(mCurrentMaterial, OBJSubMesh());
        mCurrentSubMesh = &ret.first->second;
    }

    const size_t numVertices = tokens.size() - 1;

    if (numVertices != 3 && numVertices != 4)
    {
        LogError("%s: %u: Expected 3 or 4 vertices", mPath, mCurrentLine);
        return false;
    }

    /* Each face gives 3 or 4 vertices as a set of indices into the sets of
     * vertex elements that have been declared. */
    uint16_t indices[4];
    for (size_t i = 0; i < numVertices; i++)
    {
        std::vector<std::string> subTokens;
        StringUtils::Tokenize(tokens[i + 1], subTokens, "/", -1, false);
        if (subTokens.size() != 3)
        {
            LogError("%s: %u: Expected v/vt/vn", mPath, mCurrentLine);
            return false;
        }

        /* Get the vertex elements referred to by this vertex. */
        OBJVertexKey key;
        for (size_t j = 0; j < 3; j++)
        {
            const char* const subToken = subTokens[j].c_str();
            char* end;

            uint16_t value = strtoul(subToken, &end, 10);
            if (end != subToken + subTokens[j].length())
            {
                LogError("%s: %u: Expected integer value", mPath, mCurrentLine);
                return false;
            }

            /* Indices are 1 based. */
            value -= 1;

            switch (j)
            {
                case 0:
                    if (value > mPositions.size())
                    {
                        LogError("%s: %u: Invalid position index %u", mPath, mCurrentLine, value);
                        return false;
                    }

                    key.position = value;
                    break;

                case 1:
                    if (value > mTexcoords.size())
                    {
                        LogError("%s: %u: Invalid texture coordinate index %u", mPath, mCurrentLine, value);
                        return false;
                    }

                    key.texcoord = value;
                    break;

                case 2:
                    if (value > mNormals.size())
                    {
                        LogError("%s: %u: Invalid normal index %u", mPath, mCurrentLine, value);
                        return false;
                    }

                    key.normal = value;
                    break;

            }
        }

        /* Add the vertex if we don't already have it. */
        auto ret = mVertexMap.emplace(key, mVertexCount);
        if (ret.second)
        {
            mVertexCount++;
        }

        indices[i] = ret.first->second;
    }

    /* Add the indices. If there's 4 it's a quad so add it as 2 triangles. */
    mCurrentSubMesh->indices.emplace_back(indices[0]);
    mCurrentSubMesh->indices.emplace_back(indices[1]);
    mCurrentSubMesh->indices.emplace_back(indices[2]);
    if (numVertices == 4)
    {
        mCurrentSubMesh->indices.emplace_back(indices[2]);
        mCurrentSubMesh->indices.emplace_back(indices[3]);
        mCurrentSubMesh->indices.emplace_back(indices[0]);
    }

    return true;
}

AssetPtr OBJLoader::BuildMesh()
{
    if (mVertexCount == 0)
    {
        LogError("%s: No vertices defined", mPath);
        return nullptr;
    }

    MeshPtr mesh(new Mesh());

    GPUVertexInputStateDesc inputDesc;

    inputDesc.buffers[0].stride = sizeof(OBJVertex);

    inputDesc.attributes[0].semantic = kGPUAttributeSemantic_Position;
    inputDesc.attributes[0].format   = kGPUAttributeFormat_R32G32B32_Float;
    inputDesc.attributes[0].buffer   = 0;
    inputDesc.attributes[0].offset   = offsetof(OBJVertex, position);

    inputDesc.attributes[1].semantic = kGPUAttributeSemantic_TexCoord;
    inputDesc.attributes[1].format   = kGPUAttributeFormat_R32G32_Float;
    inputDesc.attributes[1].buffer   = 0;
    inputDesc.attributes[1].offset   = offsetof(OBJVertex, texcoord);

    inputDesc.attributes[2].semantic = kGPUAttributeSemantic_Normal;
    inputDesc.attributes[2].format   = kGPUAttributeFormat_R32G32B32_Float;
    inputDesc.attributes[2].buffer   = 0;
    inputDesc.attributes[2].offset   = offsetof(OBJVertex, normal);

    mesh->SetVertexLayout(inputDesc, mVertexCount);

    /* Build vertex data. */
    ByteArray vertexDataArray(sizeof(OBJVertex) * mVertexCount);
    auto vertexData = reinterpret_cast<OBJVertex*>(vertexDataArray.Get());

    for (const auto& vertex : mVertexMap)
    {
        const size_t index = vertex.second;

        vertexData[index].position = mPositions[vertex.first.position];
        vertexData[index].texcoord = mTexcoords[vertex.first.texcoord];
        vertexData[index].normal   = mNormals  [vertex.first.normal];
    }

    mesh->SetVertexData(0, std::move(vertexDataArray));

    /* Add submeshes. */
    for (const auto& subMesh : mSubMeshMap)
    {
        const uint32_t materialIndex = mesh->AddMaterial(subMesh.first);

        mesh->AddIndexedSubMesh(materialIndex,
                                kGPUPrimitiveTopology_TriangleList,
                                subMesh.second.indices.size(),
                                kGPUIndexType_16,
                                subMesh.second.indices.data());
    }

    mesh->Build();

    return mesh;
}
