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

void Mesh::Serialise(Serialiser& inSerialiser) const
{
    Asset::Serialise(inSerialiser);

    /* Currently we have a limitation that meshes can only be serialised before
     * building them, because we discard the CPU-side copy of the vertex/index
     * data after building. In future if we need this to work, we could do a
     * GPU readback. */
    Assert(!mIsBuilt);

    inSerialiser.Write("vertexCount", mVertexCount);

    inSerialiser.BeginGroup("vertexInputState");

    Assert(mVertexInputState);

    {
        const GPUVertexInputStateDesc& inputDesc = mVertexInputState->GetDesc();

        inSerialiser.BeginArray("attributes");

        for (const auto& attribute : inputDesc.attributes)
        {
            if (attribute.semantic == kGPUAttributeSemantic_Unknown)
            {
                break;
            }

            inSerialiser.BeginGroup();

            inSerialiser.Write("semantic", attribute.semantic);
            inSerialiser.Write("index",    attribute.index);
            inSerialiser.Write("format",   attribute.format);
            inSerialiser.Write("buffer",   attribute.buffer);
            inSerialiser.Write("offset",   attribute.offset);

            inSerialiser.EndGroup();
        }

        inSerialiser.EndArray();

        inSerialiser.BeginArray("buffers");

        for (uint32_t bufferIndex = 0; bufferIndex < kMaxVertexAttributes; bufferIndex++)
        {
            if (mUsedVertexBuffers.test(bufferIndex))
            {
                const auto& buffer = inputDesc.buffers[bufferIndex];

                inSerialiser.BeginGroup();

                inSerialiser.Write("index",       bufferIndex);
                inSerialiser.Write("stride",      buffer.stride);
                inSerialiser.Write("perInstance", buffer.perInstance);

                inSerialiser.EndGroup();
            }
        }

        inSerialiser.EndArray();
    }

    inSerialiser.EndGroup();

    inSerialiser.BeginArray("materials");

    for (const std::string& material : mMaterials)
    {
        inSerialiser.Push(material);
    }

    inSerialiser.EndArray();

    inSerialiser.BeginArray("vertexData");

    for (uint32_t bufferIndex = 0; bufferIndex < kMaxVertexAttributes; bufferIndex++)
    {
        if (mUsedVertexBuffers.test(bufferIndex))
        {
            inSerialiser.BeginGroup();

            inSerialiser.Write("index", bufferIndex);
            inSerialiser.WriteBinary("data", mVertexData[bufferIndex]);

            inSerialiser.EndGroup();
        }
    }

    inSerialiser.EndArray();

    inSerialiser.BeginArray("subMeshes");

    for (const SubMesh* const subMesh : mSubMeshes)
    {
        inSerialiser.BeginGroup();

        inSerialiser.Write("material", subMesh->mMaterial);
        inSerialiser.Write("topology", subMesh->mTopology);
        inSerialiser.Write("indexed",  subMesh->mIndexed);
        inSerialiser.Write("count",    subMesh->mCount);

        if (subMesh->mIndexed)
        {
            inSerialiser.Write("indexType", subMesh->mIndexType);
            inSerialiser.WriteBinary("indexData", subMesh->mIndexData);
        }
        else
        {
            inSerialiser.Write("vertexOffset", subMesh->mVertexOffset);
        }

        inSerialiser.EndGroup();
    }

    inSerialiser.EndArray();
}

void Mesh::Deserialise(Serialiser& inSerialiser)
{
    Asset::Deserialise(inSerialiser);

    bool success = true;
    Unused(success);

    GPUVertexInputStateDesc inputDesc;
    uint32_t vertexCount;
    inSerialiser.Read("vertexCount", vertexCount);

    success &= inSerialiser.BeginGroup("vertexInputState");
    Assert(success);

    {
        success &= inSerialiser.BeginArray("attributes");
        Assert(success);

        uint32_t attributeIndex = 0;

        while (inSerialiser.BeginGroup())
        {
            auto& attribute = inputDesc.attributes[attributeIndex++];

            success &= inSerialiser.Read("semantic", attribute.semantic);
            success &= inSerialiser.Read("index",    attribute.index);
            success &= inSerialiser.Read("format",   attribute.format);
            success &= inSerialiser.Read("buffer",   attribute.buffer);
            success &= inSerialiser.Read("offset",   attribute.offset);
            Assert(success);

            inSerialiser.EndGroup();
        }

        inSerialiser.EndArray();

        success &= inSerialiser.BeginArray("buffers");
        Assert(success);

        while (inSerialiser.BeginGroup())
        {
            uint32_t bufferIndex;
            success &= inSerialiser.Read("index", bufferIndex);
            Assert(success);

            auto& buffer = inputDesc.buffers[bufferIndex];

            inSerialiser.Read("stride",      buffer.stride);
            inSerialiser.Read("perInstance", buffer.perInstance);

            inSerialiser.EndGroup();
        }

        inSerialiser.EndArray();
    }

    inSerialiser.EndGroup();

    SetVertexLayout(inputDesc, vertexCount);

    success &= inSerialiser.BeginArray("materials");
    Assert(success);

    std::string material;
    while (inSerialiser.Pop(material))
    {
        mMaterials.emplace_back(std::move(material));
    }

    inSerialiser.EndArray();

    success &= inSerialiser.BeginArray("vertexData");
    Assert(success);

    while (inSerialiser.BeginGroup())
    {
        uint32_t bufferIndex;
        success &= inSerialiser.Read("index", bufferIndex);
        Assert(success);
        Assert(mUsedVertexBuffers.test(bufferIndex));

        success &= inSerialiser.ReadBinary("data", mVertexData[bufferIndex]);
        Assert(success);

        inSerialiser.EndGroup();
    }

    inSerialiser.EndArray();

    success &= inSerialiser.BeginArray("subMeshes");
    Assert(success);

    while (inSerialiser.BeginGroup())
    {
        auto subMesh = new SubMesh(*this);
        subMesh->mIndexBuffer = nullptr;

        success &= inSerialiser.Read("material", subMesh->mMaterial);
        success &= inSerialiser.Read("topology", subMesh->mTopology);
        success &= inSerialiser.Read("indexed",  subMesh->mIndexed);
        success &= inSerialiser.Read("count",    subMesh->mCount);

        if (subMesh->mIndexed)
        {
            success &= inSerialiser.Read("indexType", subMesh->mIndexType);
            success &= inSerialiser.ReadBinary("indexData", subMesh->mIndexData);
        }
        else
        {
            success &= inSerialiser.Read("vertexOffset", subMesh->mVertexOffset);
        }

        Assert(success);

        mSubMeshes.emplace_back(subMesh);

        inSerialiser.EndGroup();
    }

    inSerialiser.EndArray();

    Build();
}

bool Mesh::GetMaterial(const std::string& inName,
                       size_t&            outIndex) const
{
    for (size_t i = 0; i < mMaterials.size(); i++)
    {
        if (mMaterials[i] == inName)
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}

void Mesh::SetVertexLayout(const GPUVertexInputStateDesc& inDesc,
                           const uint32_t                 inCount)
{
    Assert(!mIsBuilt);
    Assert(!mVertexInputState);
    Assert(inCount > 0);

    mVertexInputState = GPUVertexInputState::Get(inDesc);
    mVertexCount      = inCount;

    bool hasPosition = false;

    for (size_t i = 0; i < kMaxVertexAttributes; i++)
    {
        const auto& attribute = inDesc.attributes[i];

        if (attribute.semantic != kGPUAttributeSemantic_Unknown)
        {
            mUsedVertexBuffers.set(attribute.buffer);

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

void Mesh::SetVertexData(const uint32_t inIndex,
                         ByteArray      inData)
{
    Assert(!mIsBuilt);
    Assert(mUsedVertexBuffers.test(inIndex));
    Assert(inData.GetSize() == mVertexInputState->GetDesc().buffers[inIndex].stride * mVertexCount);

    mVertexData[inIndex] = std::move(inData);
}

void Mesh::SetVertexData(const uint32_t    inIndex,
                         const void* const inData)
{
    Assert(mVertexInputState);

    const size_t size = mVertexInputState->GetDesc().buffers[inIndex].stride * mVertexCount;

    ByteArray data(size);
    memcpy(data.Get(), inData, size);

    SetVertexData(inIndex, std::move(data));
}

uint32_t Mesh::AddMaterial(std::string inName)
{
    Assert(!mIsBuilt);

    size_t index;
    Assert(!GetMaterial(inName, index));

    index = mMaterials.size();
    mMaterials.emplace_back(std::move(inName));

    return index;
}

void Mesh::AddSubMesh(const uint32_t             inMaterialIndex,
                      const GPUPrimitiveTopology inTopology,
                      const uint32_t             inVertexOffset,
                      const uint32_t             inVertexCount)
{
    Assert(!mIsBuilt);
    Assert(inMaterialIndex < mMaterials.size());
    Assert(inVertexCount > 0);
    Assert(inVertexOffset + inVertexCount <= mVertexCount);

    auto subMesh = new SubMesh(*this);

    subMesh->mMaterial     = inMaterialIndex;
    subMesh->mTopology     = inTopology;
    subMesh->mIndexed      = false;
    subMesh->mCount        = inVertexCount;
    subMesh->mVertexOffset = inVertexOffset;
    subMesh->mIndexBuffer  = nullptr;

    mSubMeshes.emplace_back(subMesh);
}

void Mesh::AddIndexedSubMesh(const uint32_t             inMaterialIndex,
                             const GPUPrimitiveTopology inTopology,
                             const uint32_t             inIndexCount,
                             const GPUIndexType         inIndexType,
                             ByteArray                  inIndexData)
{
    Assert(!mIsBuilt);
    Assert(inMaterialIndex < mMaterials.size());
    Assert(inIndexData.GetSize() == GPUUtils::GetIndexSize(inIndexType) * inIndexCount);

    auto subMesh = new SubMesh(*this);

    subMesh->mMaterial    = inMaterialIndex;
    subMesh->mTopology    = inTopology;
    subMesh->mIndexed     = true;
    subMesh->mCount       = inIndexCount;
    subMesh->mIndexType   = inIndexType;
    subMesh->mIndexBuffer = nullptr;
    subMesh->mIndexData   = std::move(inIndexData);

    mSubMeshes.emplace_back(subMesh);
}

void Mesh::AddIndexedSubMesh(const uint32_t             inMaterialIndex,
                             const GPUPrimitiveTopology inTopology,
                             const uint32_t             inIndexCount,
                             const GPUIndexType         inIndexType,
                             const void* const          inIndexData)
{
    const size_t size = GPUUtils::GetIndexSize(inIndexType) * inIndexCount;

    ByteArray data(size);
    memcpy(data.Get(), inIndexData, size);

    AddIndexedSubMesh(inMaterialIndex,
                      inTopology,
                      inIndexCount,
                      inIndexType,
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
        if (mUsedVertexBuffers.test(i))
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
}

void Mesh::CalculateBoundingBox(SubMesh* const inSubMesh)
{
    Assert(inSubMesh->mCount != 0);

    glm::vec3 minimum( std::numeric_limits<float>::max());
    glm::vec3 maximum(-std::numeric_limits<float>::max());

    for (uint32_t i = 0; i < inSubMesh->mCount; i++)
    {
        const glm::vec3 position = glm::vec3(LoadAttribute(kGPUAttributeSemantic_Position, 0, inSubMesh, i));

        minimum = glm::min(minimum, position);
        maximum = glm::max(maximum, position);
    }

    inSubMesh->mBoundingBox = BoundingBox(minimum, maximum);
}

glm::vec4 Mesh::LoadAttribute(const GPUAttributeSemantic inSemantic,
                              const uint8_t              inSemanticIndex,
                              const SubMesh* const       inSubMesh,
                              const uint32_t             inIndex)
{
    Assert(inIndex < inSubMesh->mCount);

    uint32_t vertexIndex;

    if (inSubMesh->mIndexed)
    {
        switch (inSubMesh->mIndexType)
        {
            case kGPUIndexType_16:
            {
                vertexIndex = reinterpret_cast<const uint16_t*>(inSubMesh->mIndexData.Get())[inIndex];
                break;
            }

            case kGPUIndexType_32:
            {
                vertexIndex = reinterpret_cast<const uint32_t*>(inSubMesh->mIndexData.Get())[inIndex];
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
        vertexIndex = inSubMesh->mVertexOffset + inIndex;
    }

    return LoadAttribute(inSemantic, inSemanticIndex, vertexIndex);
}

glm::vec4 Mesh::LoadAttribute(const GPUAttributeSemantic inSemantic,
                              const uint8_t              inSemanticIndex,
                              const uint32_t             inVertexIndex)
{
    Assert(inVertexIndex < mVertexCount);

    const GPUVertexInputStateDesc::Attribute* attributeDesc =
        mVertexInputState->GetDesc().FindAttribute(inSemantic, inSemanticIndex);

    Assert(attributeDesc);

    const GPUVertexInputStateDesc::Buffer* bufferDesc =
        &mVertexInputState->GetDesc().buffers[attributeDesc->buffer];

    Assert(!bufferDesc->perInstance);
    Assert(mVertexData[attributeDesc->buffer]);

    const auto data =
        reinterpret_cast<const float*>(mVertexData[attributeDesc->buffer].Get() +
                                       (inVertexIndex * bufferDesc->stride) +
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
