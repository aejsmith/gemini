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

#include "Engine/Mesh.h"

#include "Engine/Serialiser.h"

#include "GPU/GPUBuffer.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUUtils.h"

Mesh::Mesh() :
    mIsBuilt            (false),
    mVertexInputState   (nullptr),
    mVertexCount        (0),
    mVertexBuffers      {}
{
}

Mesh::~Mesh()
{
    for (SubMesh* subMesh : mSubMeshes)
    {
        delete subMesh->mIndexBuffer;
        delete subMesh;
    }

    for (GPUBuffer* vertexBuffer : mVertexBuffers)
    {
        delete vertexBuffer;
    }
}

void Mesh::Serialise(Serialiser& serialiser) const
{
    Asset::Serialise(serialiser);

    /* Currently we have a limitation that meshes can only be serialised before
     * building them, because we discard the CPU-side copy of the vertex/index
     * data after building. In future if we need this to work, we could do a
     * GPU readback. */
    Assert(!mIsBuilt);

    serialiser.Write("vertexCount", mVertexCount);

    serialiser.BeginGroup("vertexInputState");

    Assert(mVertexInputState);

    {
        const GPUVertexInputStateDesc& inputDesc = mVertexInputState->GetDesc();

        serialiser.BeginArray("attributes");

        for (const auto& attribute : inputDesc.attributes)
        {
            if (attribute.semantic == kGPUAttributeSemantic_Unknown)
            {
                break;
            }

            serialiser.BeginGroup();

            serialiser.Write("semantic", attribute.semantic);
            serialiser.Write("index",    attribute.index);
            serialiser.Write("format",   attribute.format);
            serialiser.Write("buffer",   attribute.buffer);
            serialiser.Write("offset",   attribute.offset);

            serialiser.EndGroup();
        }

        serialiser.EndArray();

        serialiser.BeginArray("buffers");

        for (uint32_t bufferIndex = 0; bufferIndex < kMaxVertexAttributes; bufferIndex++)
        {
            if (mUsedVertexBuffers.Test(bufferIndex))
            {
                const auto& buffer = inputDesc.buffers[bufferIndex];

                serialiser.BeginGroup();

                serialiser.Write("index",       bufferIndex);
                serialiser.Write("stride",      buffer.stride);
                serialiser.Write("perInstance", buffer.perInstance);

                serialiser.EndGroup();
            }
        }

        serialiser.EndArray();
    }

    serialiser.EndGroup();

    serialiser.BeginArray("materials");

    for (const std::string& material : mMaterials)
    {
        serialiser.Push(material);
    }

    serialiser.EndArray();

    serialiser.BeginArray("vertexData");

    for (uint32_t bufferIndex = 0; bufferIndex < kMaxVertexAttributes; bufferIndex++)
    {
        if (mUsedVertexBuffers.Test(bufferIndex))
        {
            serialiser.BeginGroup();

            serialiser.Write("index", bufferIndex);
            serialiser.WriteBinary("data", mVertexData[bufferIndex]);

            serialiser.EndGroup();
        }
    }

    serialiser.EndArray();

    serialiser.BeginArray("subMeshes");

    for (const SubMesh* const subMesh : mSubMeshes)
    {
        serialiser.BeginGroup();

        serialiser.Write("material", subMesh->mMaterial);
        serialiser.Write("topology", subMesh->mTopology);
        serialiser.Write("indexed",  subMesh->mIndexed);
        serialiser.Write("count",    subMesh->mCount);

        if (subMesh->mIndexed)
        {
            serialiser.Write("indexType", subMesh->mIndexType);
            serialiser.WriteBinary("indexData", subMesh->mIndexData);
        }
        else
        {
            serialiser.Write("vertexOffset", subMesh->mVertexOffset);
        }

        serialiser.EndGroup();
    }

    serialiser.EndArray();
}

void Mesh::Deserialise(Serialiser& serialiser)
{
    Asset::Deserialise(serialiser);

    bool success = true;
    Unused(success);

    GPUVertexInputStateDesc inputDesc;
    uint32_t vertexCount;
    serialiser.Read("vertexCount", vertexCount);

    success &= serialiser.BeginGroup("vertexInputState");
    Assert(success);

    {
        success &= serialiser.BeginArray("attributes");
        Assert(success);

        uint32_t attributeIndex = 0;

        while (serialiser.BeginGroup())
        {
            auto& attribute = inputDesc.attributes[attributeIndex++];

            success &= serialiser.Read("semantic", attribute.semantic);
            success &= serialiser.Read("index",    attribute.index);
            success &= serialiser.Read("format",   attribute.format);
            success &= serialiser.Read("buffer",   attribute.buffer);
            success &= serialiser.Read("offset",   attribute.offset);
            Assert(success);

            serialiser.EndGroup();
        }

        serialiser.EndArray();

        success &= serialiser.BeginArray("buffers");
        Assert(success);

        while (serialiser.BeginGroup())
        {
            uint32_t bufferIndex;
            success &= serialiser.Read("index", bufferIndex);
            Assert(success);

            auto& buffer = inputDesc.buffers[bufferIndex];

            serialiser.Read("stride",      buffer.stride);
            serialiser.Read("perInstance", buffer.perInstance);

            serialiser.EndGroup();
        }

        serialiser.EndArray();
    }

    serialiser.EndGroup();

    SetVertexLayout(inputDesc, vertexCount);

    success &= serialiser.BeginArray("materials");
    Assert(success);

    std::string material;
    while (serialiser.Pop(material))
    {
        mMaterials.emplace_back(std::move(material));
    }

    serialiser.EndArray();

    success &= serialiser.BeginArray("vertexData");
    Assert(success);

    while (serialiser.BeginGroup())
    {
        uint32_t bufferIndex;
        success &= serialiser.Read("index", bufferIndex);
        Assert(success);
        Assert(mUsedVertexBuffers.Test(bufferIndex));

        success &= serialiser.ReadBinary("data", mVertexData[bufferIndex]);
        Assert(success);

        serialiser.EndGroup();
    }

    serialiser.EndArray();

    success &= serialiser.BeginArray("subMeshes");
    Assert(success);

    while (serialiser.BeginGroup())
    {
        auto subMesh = new SubMesh(*this);
        subMesh->mIndexBuffer = nullptr;

        success &= serialiser.Read("material", subMesh->mMaterial);
        success &= serialiser.Read("topology", subMesh->mTopology);
        success &= serialiser.Read("indexed",  subMesh->mIndexed);
        success &= serialiser.Read("count",    subMesh->mCount);

        if (subMesh->mIndexed)
        {
            success &= serialiser.Read("indexType", subMesh->mIndexType);
            success &= serialiser.ReadBinary("indexData", subMesh->mIndexData);
        }
        else
        {
            success &= serialiser.Read("vertexOffset", subMesh->mVertexOffset);
        }

        Assert(success);

        mSubMeshes.emplace_back(subMesh);

        serialiser.EndGroup();
    }

    serialiser.EndArray();

    Build();
}

void Mesh::PathChanged()
{
    if (GEMINI_BUILD_DEBUG && IsManaged() && mIsBuilt)
    {
        for (size_t i = 0; i < kMaxVertexAttributes; i++)
        {
            if (mVertexBuffers[i])
            {
                mVertexBuffers[i]->SetName(StringUtils::Format("%s (Buffer %zu)",
                                                               GetPath().c_str(),
                                                               i));
            }
        }

        for (size_t i = 0; i < mSubMeshes.size(); i++)
        {
            if (mSubMeshes[i]->mIndexBuffer)
            {
                mSubMeshes[i]->mIndexBuffer->SetName(StringUtils::Format("%s (SubMesh %zu)",
                                                                         GetPath().c_str(),
                                                                         i));
            }
        }
    }
}

bool Mesh::GetMaterial(const std::string& name,
                       size_t&            outIndex) const
{
    for (size_t i = 0; i < mMaterials.size(); i++)
    {
        if (mMaterials[i] == name)
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}

void Mesh::SetVertexLayout(const GPUVertexInputStateDesc& desc,
                           const uint32_t                 count)
{
    Assert(!mIsBuilt);
    Assert(!mVertexInputState);
    Assert(count > 0);

    mVertexInputState = GPUVertexInputState::Get(desc);
    mVertexCount      = count;

    bool hasPosition = false;

    for (size_t i = 0; i < kMaxVertexAttributes; i++)
    {
        const auto& attribute = desc.attributes[i];

        if (attribute.semantic != kGPUAttributeSemantic_Unknown)
        {
            mUsedVertexBuffers.Set(attribute.buffer);

            if (attribute.semantic == kGPUAttributeSemantic_Position)
            {
                AssertMsg(!hasPosition && attribute.index == 0,
                          "Vertex layout must have only one position at index 0");

                hasPosition = true;
            }
        }
    }

    AssertMsg(hasPosition, "Vertex layout must have a position");
}

void Mesh::SetVertexData(const uint32_t index,
                         ByteArray      data)
{
    Assert(!mIsBuilt);
    Assert(mUsedVertexBuffers.Test(index));
    Assert(data.GetSize() == mVertexInputState->GetDesc().buffers[index].stride * mVertexCount);

    mVertexData[index] = std::move(data);
}

void Mesh::SetVertexData(const uint32_t    index,
                         const void* const data)
{
    Assert(mVertexInputState);

    const size_t size = mVertexInputState->GetDesc().buffers[index].stride * mVertexCount;

    ByteArray bytes(size);
    memcpy(bytes.Get(), data, size);

    SetVertexData(index, std::move(bytes));
}

uint32_t Mesh::AddMaterial(std::string name)
{
    Assert(!mIsBuilt);

    size_t index;
    Assert(!GetMaterial(name, index));

    index = mMaterials.size();
    mMaterials.emplace_back(std::move(name));

    return index;
}

void Mesh::AddSubMesh(const uint32_t             materialIndex,
                      const GPUPrimitiveTopology topology,
                      const uint32_t             vertexOffset,
                      const uint32_t             vertexCount)
{
    Assert(!mIsBuilt);
    Assert(materialIndex < mMaterials.size());
    Assert(vertexCount > 0);
    Assert(vertexOffset + vertexCount <= mVertexCount);

    auto subMesh = new SubMesh(*this);

    subMesh->mMaterial     = materialIndex;
    subMesh->mTopology     = topology;
    subMesh->mIndexed      = false;
    subMesh->mCount        = vertexCount;
    subMesh->mVertexOffset = vertexOffset;
    subMesh->mIndexBuffer  = nullptr;

    mSubMeshes.emplace_back(subMesh);
}

void Mesh::AddIndexedSubMesh(const uint32_t             materialIndex,
                             const GPUPrimitiveTopology topology,
                             const uint32_t             indexCount,
                             const GPUIndexType         indexType,
                             ByteArray                  indexData)
{
    Assert(!mIsBuilt);
    Assert(materialIndex < mMaterials.size());
    Assert(indexData.GetSize() == GPUUtils::GetIndexSize(indexType) * indexCount);

    auto subMesh = new SubMesh(*this);

    subMesh->mMaterial    = materialIndex;
    subMesh->mTopology    = topology;
    subMesh->mIndexed     = true;
    subMesh->mCount       = indexCount;
    subMesh->mIndexType   = indexType;
    subMesh->mIndexBuffer = nullptr;
    subMesh->mIndexData   = std::move(indexData);

    mSubMeshes.emplace_back(subMesh);
}

void Mesh::AddIndexedSubMesh(const uint32_t             materialIndex,
                             const GPUPrimitiveTopology topology,
                             const uint32_t             indexCount,
                             const GPUIndexType         indexType,
                             const void* const          indexData)
{
    const size_t size = GPUUtils::GetIndexSize(indexType) * indexCount;

    ByteArray data(size);
    memcpy(data.Get(), indexData, size);

    AddIndexedSubMesh(materialIndex,
                      topology,
                      indexCount,
                      indexType,
                      std::move(data));
}

void Mesh::Build()
{
    Assert(!mIsBuilt);
    Assert(mVertexInputState);
    Assert(mVertexCount > 0);
    Assert(mMaterials.size() > 0);
    Assert(mSubMeshes.size() > 0);

    /* For now, since we must use the main thread to do the GPU buffer upload.
     * TODO: Allow asynchronous resource creation which uploads via a command
     * list. Also use transfer queue when available. */
    Assert(Thread::IsMain());

    for (SubMesh* subMesh : mSubMeshes)
    {
        CalculateBoundingBox(subMesh);

        if (subMesh->mIndexed)
        {
            GPUBufferDesc bufferDesc;
            bufferDesc.usage = kGPUResourceUsage_Standard;
            bufferDesc.size  = subMesh->mIndexData.GetSize();

            subMesh->mIndexBuffer = GPUDevice::Get().CreateBuffer(bufferDesc);

            GPUStagingBuffer stagingBuffer(kGPUStagingAccess_Write, bufferDesc.size);
            stagingBuffer.Write(subMesh->mIndexData.Get(), bufferDesc.size);
            stagingBuffer.Finalise();

            GPUGraphicsContext::Get().UploadBuffer(subMesh->mIndexBuffer,
                                                   stagingBuffer,
                                                   bufferDesc.size);

            GPUGraphicsContext::Get().ResourceBarrier(subMesh->mIndexBuffer,
                                                      kGPUResourceState_TransferWrite,
                                                      kGPUResourceState_IndexBufferRead);

            subMesh->mIndexData.Clear();
        }
    }

    for (size_t i = 0; i < kMaxVertexAttributes; i++)
    {
        if (mUsedVertexBuffers.Test(i))
        {
            Assert(mVertexData[i]);

            GPUBufferDesc bufferDesc;
            bufferDesc.usage = kGPUResourceUsage_Standard;
            bufferDesc.size  = mVertexData[i].GetSize();

            mVertexBuffers[i] = GPUDevice::Get().CreateBuffer(bufferDesc);

            GPUStagingBuffer stagingBuffer(kGPUStagingAccess_Write, bufferDesc.size);
            stagingBuffer.Write(mVertexData[i].Get(), bufferDesc.size);
            stagingBuffer.Finalise();

            GPUGraphicsContext::Get().UploadBuffer(mVertexBuffers[i],
                                                   stagingBuffer,
                                                   bufferDesc.size);

            GPUGraphicsContext::Get().ResourceBarrier(mVertexBuffers[i],
                                                      kGPUResourceState_TransferWrite,
                                                      kGPUResourceState_VertexBufferRead);

            mVertexData[i].Clear();
        }
    }

    mIsBuilt = true;

    PathChanged();
}

void Mesh::CalculateBoundingBox(SubMesh* const subMesh)
{
    Assert(subMesh->mCount != 0);

    glm::vec3 minimum( std::numeric_limits<float>::max());
    glm::vec3 maximum(-std::numeric_limits<float>::max());

    for (uint32_t i = 0; i < subMesh->mCount; i++)
    {
        const glm::vec3 position = glm::vec3(LoadAttribute(kGPUAttributeSemantic_Position, 0, subMesh, i));

        minimum = glm::min(minimum, position);
        maximum = glm::max(maximum, position);
    }

    subMesh->mBoundingBox = BoundingBox(minimum, maximum);
}

glm::vec4 Mesh::LoadAttribute(const GPUAttributeSemantic semantic,
                              const uint8_t              semanticIndex,
                              const SubMesh* const       subMesh,
                              const uint32_t             index)
{
    Assert(index < subMesh->mCount);

    uint32_t vertexIndex;

    if (subMesh->mIndexed)
    {
        switch (subMesh->mIndexType)
        {
            case kGPUIndexType_16:
            {
                vertexIndex = reinterpret_cast<const uint16_t*>(subMesh->mIndexData.Get())[index];
                break;
            }

            case kGPUIndexType_32:
            {
                vertexIndex = reinterpret_cast<const uint32_t*>(subMesh->mIndexData.Get())[index];
                break;
            }

            default:
            {
                Unreachable();
            }
        }
    }
    else
    {
        vertexIndex = subMesh->mVertexOffset + index;
    }

    return LoadAttribute(semantic, semanticIndex, vertexIndex);
}

glm::vec4 Mesh::LoadAttribute(const GPUAttributeSemantic semantic,
                              const uint8_t              semanticIndex,
                              const uint32_t             vertexIndex)
{
    Assert(vertexIndex < mVertexCount);

    const GPUVertexInputStateDesc::Attribute* attributeDesc =
        mVertexInputState->GetDesc().FindAttribute(semantic, semanticIndex);

    Assert(attributeDesc);

    const GPUVertexInputStateDesc::Buffer* bufferDesc =
        &mVertexInputState->GetDesc().buffers[attributeDesc->buffer];

    Assert(!bufferDesc->perInstance);
    Assert(mVertexData[attributeDesc->buffer]);

    const auto data =
        reinterpret_cast<const float*>(mVertexData[attributeDesc->buffer].Get() +
                                       (vertexIndex * bufferDesc->stride) +
                                       attributeDesc->offset);

    /* Zero components which aren't present. */
    glm::vec4 result(0.0f);

    switch (attributeDesc->format)
    {
        case kGPUAttributeFormat_R32G32B32A32_Float:
            result.a = data[3];
        case kGPUAttributeFormat_R32G32B32_Float:
            result.b = data[2];
        case kGPUAttributeFormat_R32G32_Float:
            result.g = data[1];
        case kGPUAttributeFormat_R32_Float:
            result.r = data[0];
            break;

        default:
            UnreachableMsg("Unhandled GPUAttributeFormat");

    }

    return result;
}
