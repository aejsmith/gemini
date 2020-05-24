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

#include "Loaders/GLTFImporter.h"

#include "Core/Base64.h"
#include "Core/ByteArray.h"
#include "Core/Filesystem.h"

#include "Engine/AssetManager.h"

#include "Entity/Entity.h"

#include "GPU/GPUUtils.h"

#include "Render/MeshRenderer.h"

#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <memory>

static constexpr char kRequiredVersion[]            = "2.0";

static constexpr uint32_t GL_POINTS                 = 0;
static constexpr uint32_t GL_LINES                  = 1;
static constexpr uint32_t GL_LINE_STRIP             = 3;
static constexpr uint32_t GL_TRIANGLES              = 4;
static constexpr uint32_t GL_TRIANGLE_STRIP         = 5;
static constexpr uint32_t GL_TRIANGLE_FAN           = 6;

static constexpr uint32_t GL_UNSIGNED_BYTE          = 5121;
static constexpr uint32_t GL_UNSIGNED_SHORT         = 5123;
static constexpr uint32_t GL_FLOAT                  = 5126;

static constexpr uint32_t GL_CLAMP_TO_EDGE          = 33071;
static constexpr uint32_t GL_MIRRORED_REPEAT        = 33648;
static constexpr uint32_t GL_REPEAT                 = 10497;

static constexpr uint32_t kInvalidIndex     = std::numeric_limits<uint32_t>::max();

static float GetFloat(const rapidjson::Value& entry,
                      const char* const       name,
                      const float             defaultValue)
{
    float result = defaultValue;

    if (entry.HasMember(name))
    {
        result = entry[name].GetFloat();
    }

    return result;
}

static glm::vec3 GetVec3(const rapidjson::Value& entry,
                         const char* const       name,
                         const glm::vec3&        defaultValue)
{
    glm::vec3 result = defaultValue;

    if (entry.HasMember(name))
    {
        const rapidjson::Value& value = entry[name];

        result.x = value[0u].GetFloat();
        result.y = value[1u].GetFloat();
        result.z = value[2u].GetFloat();
    }

    return result;
}

static glm::vec4 GetVec4(const rapidjson::Value& entry,
                         const char* const       name,
                         const glm::vec4&        defaultValue)
{
    glm::vec4 result = defaultValue;

    if (entry.HasMember(name))
    {
        const rapidjson::Value& value = entry[name];

        result.x = value[0u].GetFloat();
        result.y = value[1u].GetFloat();
        result.z = value[2u].GetFloat();
        result.w = value[3u].GetFloat();
    }

    return result;
}

static glm::quat GetQuat(const rapidjson::Value& entry,
                         const char* const       name,
                         const glm::quat&        defaultValue)
{
    glm::quat result = defaultValue;

    if (entry.HasMember(name))
    {
        const rapidjson::Value& value = entry[name];

        result.x = value[0u].GetFloat();
        result.y = value[1u].GetFloat();
        result.z = value[2u].GetFloat();
        result.w = value[3u].GetFloat();
    }

    return result;
}

GLTFImporter::GLTFImporter()
{
}

GLTFImporter::~GLTFImporter()
{
}

bool GLTFImporter::Import(const Path&  path,
                          const Path&  assetDir,
                          World* const world)
{
    mPath     = path;
    mAssetDir = assetDir;
    mWorld    = world;

    /* Parse the file content. */
    {
        UPtr<DataStream> file(Filesystem::OpenFile(mPath));
        if (!file)
        {
            LogError("%s: Failed to open file", mPath.GetCString());
            return false;
        }

        ByteArray data(file->GetSize());
        if (!file->Read(data.Get(), data.GetSize()))
        {
            LogError("%s: Failed to read file", mPath.GetCString());
            return false;
        }

        mDocument.Parse(reinterpret_cast<const char*>(data.Get()), data.GetSize());
        if (mDocument.HasParseError())
        {
            LogError("%s: Parse error at %zu: %s",
                     mPath.GetCString(),
                     mDocument.GetErrorOffset(),
                     rapidjson::GetParseError_En(mDocument.GetParseError()));

            return false;
        }
    }

    if (!mDocument.IsObject())
    {
        LogError("%s: Document root is not an object", mPath.GetCString());
        return false;
    }
    else if (!mDocument.HasMember("asset"))
    {
        LogError("%s: 'asset' property does not exist", mPath.GetCString());
        return false;
    }

    const rapidjson::Value& asset = mDocument["asset"];

    if (!asset.IsObject() || !asset.HasMember("version"))
    {
        LogError("%s: 'asset' property is invalid", mPath.GetCString());
        return false;
    }
    else if (strcmp(asset["version"].GetString(), kRequiredVersion) != 0)
    {
        LogError("%s: Asset version '%s' is unsupported", asset["version"].GetString());
        return false;
    }

    if (mDocument.HasMember("extensionsRequired"))
    {
        // TODO
        LogError("%s: Extensions are required which are not currently supported", mPath.GetCString());
        return false;
    }

    /* Load and validate everything from the file bottom-up. */
    if (!LoadBuffers())     return false;
    if (!LoadBufferViews()) return false;
    if (!LoadAccessors())   return false;
    if (!LoadSamplers())    return false;
    if (!LoadImages())      return false;
    if (!LoadTextures())    return false;
    if (!LoadMaterials())   return false;
    if (!LoadMeshes())      return false;
    if (!LoadNodes())       return false;
    if (!LoadScene())       return false;

    /* Generate the world top-down from what's actually required for the
     * specified scene. */
    if (!GenerateScene())   return false;

    return true;
}

bool GLTFImporter::LoadAccessors()
{
    if (!mDocument.HasMember("accessors"))
    {
        return true;
    }

    const rapidjson::Value& accessors = mDocument["accessors"];

    if (!accessors.IsArray())
    {
        LogError("%s: 'accessors' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : accessors.GetArray())
    {
        // TODO: Buffer view can be unspecified in which case a buffer should
        // be filled with zeros.
        if (!entry.IsObject() ||
            !entry.HasMember("bufferView")    || !entry["bufferView"].IsUint() ||
            !entry.HasMember("componentType") || !entry["componentType"].IsUint() ||
            !entry.HasMember("count")         || !entry["count"].IsUint() ||
            !entry.HasMember("type")          || !entry["type"].IsString() ||
            (entry.HasMember("byteOffset")    && !entry["byteOffset"].IsUint64()) ||
            (entry.HasMember("normalized")    && !entry["normalized"].IsBool()))
        {
            LogError("%s: Accessor has missing/invalid properties", mPath.GetCString());
            return false;
        }

        Accessor& accessor = mAccessors.emplace_back();

        accessor.bufferView = entry["bufferView"].GetUint();
        accessor.count      = entry["count"].GetUint();
        accessor.offset     = (entry.HasMember("byteOffset")) ? entry["byteOffset"].GetUint64() : 0;

        if (accessor.bufferView >= mBufferViews.size())
        {
            LogError("%s: Buffer view %u does not exist", mPath.GetCString(), accessor.bufferView);
            return false;
        }
        else if (accessor.count == 0)
        {
            LogError("%s: Accessor count must be non-zero", mPath.GetCString());
            return false;
        }

        const uint32_t componentType = entry["componentType"].GetUint();
        const char* const type       = entry["type"].GetString();
        const bool normalized        = (entry.HasMember("normalized")) ? entry["normalized"].GetBool() : false;

        if ((componentType != GL_UNSIGNED_BYTE &&
             componentType != GL_UNSIGNED_SHORT &&
             componentType != GL_FLOAT) ||
            (componentType == GL_FLOAT && normalized))
        {
            LogError("%s: Accessor has unhandled component type", mPath.GetCString());
            return false;
        }

        if (strcmp(type, "SCALAR") == 0)
        {
            switch (componentType)
            {
                case GL_UNSIGNED_BYTE:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R8_UNorm : kGPUAttributeFormat_R8_UInt;
                    break;

                case GL_UNSIGNED_SHORT:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R16_UNorm : kGPUAttributeFormat_R16_UInt;
                    break;

                case GL_FLOAT:
                    accessor.format = kGPUAttributeFormat_R32_Float;
                    break;

            }
        }
        else if (strcmp(type, "VEC2") == 0)
        {
            switch (componentType)
            {
                case GL_UNSIGNED_BYTE:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R8G8_UNorm : kGPUAttributeFormat_R8G8_UInt;
                    break;

                case GL_UNSIGNED_SHORT:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R16G16_UNorm : kGPUAttributeFormat_R16G16_UInt;
                    break;

                case GL_FLOAT:
                    accessor.format = kGPUAttributeFormat_R32G32_Float;
                    break;

            }
        }
        else if (strcmp(type, "VEC3") == 0)
        {
            switch (componentType)
            {
                case GL_UNSIGNED_BYTE:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R8G8B8_UNorm : kGPUAttributeFormat_R8G8B8_UInt;
                    break;

                case GL_UNSIGNED_SHORT:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R16G16B16_UNorm : kGPUAttributeFormat_R16G16B16_UInt;
                    break;

                case GL_FLOAT:
                    accessor.format = kGPUAttributeFormat_R32G32B32_Float;
                    break;

            }
        }
        else if (strcmp(type, "VEC4") == 0)
        {
            switch (componentType)
            {
                case GL_UNSIGNED_BYTE:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R8G8B8A8_UNorm : kGPUAttributeFormat_R8G8B8A8_UInt;
                    break;

                case GL_UNSIGNED_SHORT:
                    accessor.format = (normalized) ? kGPUAttributeFormat_R16G16B16A16_UNorm : kGPUAttributeFormat_R16G16B16A16_UInt;
                    break;

                case GL_FLOAT:
                    accessor.format = kGPUAttributeFormat_R32G32B32A32_Float;
                    break;

            }
        }
        else
        {
            LogError("%s: Accessor has unhandled type '%s'", mPath.GetCString(), type);
            return false;
        }
    }

    return true;
}

bool GLTFImporter::LoadBuffers()
{
    if (!mDocument.HasMember("buffers"))
    {
        return true;
    }

    const rapidjson::Value& buffers = mDocument["buffers"];

    if (!buffers.IsArray())
    {
        LogError("%s: 'buffers' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : buffers.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("byteLength") || !entry["byteLength"].IsUint64() ||
            !entry.HasMember("uri")        || !entry["uri"].IsString())
        {
            LogError("%s: Buffer has missing/invalid properties", mPath.GetCString());
            return false;
        }

        ByteArray& buffer = mBuffers.emplace_back();

        std::string mediaType;

        if (!LoadURI(entry["uri"], buffer, mediaType))
        {
            return false;
        }

        const uint64_t byteLength = entry["byteLength"].GetUint64();

        if (byteLength > buffer.GetSize())
        {
            LogError("%s: Buffer specifies length (%" PRIu64 ") longer than actual data (%zu)",
                     mPath.GetCString(),
                     byteLength,
                     buffer.GetSize());
            return false;
        }
        else if (byteLength < buffer.GetSize())
        {
            buffer.Resize(byteLength, false);
        }
    }

    return true;
}

bool GLTFImporter::LoadBufferViews()
{
    if (!mDocument.HasMember("bufferViews"))
    {
        return true;
    }

    const rapidjson::Value& bufferViews = mDocument["bufferViews"];

    if (!bufferViews.IsArray())
    {
        LogError("%s: 'bufferViews' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : bufferViews.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("buffer")     || !entry["buffer"].IsUint() ||
            !entry.HasMember("byteLength") || !entry["byteLength"].IsUint64() ||
            (entry.HasMember("byteOffset") && !entry["byteOffset"].IsUint64()) ||
            (entry.HasMember("byteStride") && !entry["byteStride"].IsUint64()))
        {
            LogError("%s: Buffer view has missing/invalid properties", mPath.GetCString());
            return false;
        }

        BufferView& bufferView = mBufferViews.emplace_back();

        bufferView.buffer = entry["buffer"].GetUint();
        bufferView.length = entry["byteLength"].GetUint64();
        bufferView.offset = (entry.HasMember("byteOffset")) ? entry["byteOffset"].GetUint64() : 0;
        bufferView.stride = (entry.HasMember("byteStride")) ? entry["byteStride"].GetUint64() : 0;

        if (bufferView.buffer >= mBuffers.size())
        {
            LogError("%s: Buffer %u does not exist", mPath.GetCString(), bufferView.buffer);
            return false;
        }
        else if (bufferView.offset + bufferView.length > mBuffers[bufferView.buffer].GetSize())
        {
            LogError("%s: Range %" PRIu64 " + %" PRIu64 " is outside of range of buffer %u",
                     mPath.GetCString(),
                     bufferView.offset,
                     bufferView.length,
                     bufferView.buffer);
            return false;
        }
    }

    return true;
}

bool GLTFImporter::LoadImages()
{
    if (!mDocument.HasMember("images"))
    {
        return true;
    }

    const rapidjson::Value& images = mDocument["images"];

    if (!images.IsArray())
    {
        LogError("%s: 'images' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : images.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("uri") || !entry["uri"].IsString())
        {
            LogError("%s: Image has missing/invalid properties", mPath.GetCString());
            return false;
        }

        Image& image = mImages.emplace_back();

        std::string mediaType;

        if (!LoadURI(entry["uri"], image.data, mediaType))
        {
            return false;
        }

        if (mediaType == "image/png")
        {
            image.type = kImageType_PNG;
        }
        else if (mediaType == "image/jpeg")
        {
            image.type = kImageType_JPG;
        }
        else
        {
            LogError("%s: Image has unsupported media type '%s'", mPath.GetCString(), mediaType.c_str());
            return false;
        }
    }

    return true;
}

bool GLTFImporter::LoadMaterials()
{
    if (!mDocument.HasMember("materials"))
    {
        return true;
    }

    const rapidjson::Value& materials = mDocument["materials"];

    if (!materials.IsArray())
    {
        LogError("%s: 'materials' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : materials.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("pbrMetallicRoughness") || !entry["pbrMetallicRoughness"].IsObject())
        {
            LogError("%s: Material has missing/invalid properties", mPath.GetCString());
            return false;
        }

        const rapidjson::Value& pbr = entry["pbrMetallicRoughness"];

        MaterialDef& material = mMaterials.emplace_back();

        auto GetTexture = [&] (const rapidjson::Value& parentValue,
                               const char* const       name,
                               uint32_t&               outIndex) -> bool
        {
            if (!parentValue.HasMember(name))
            {
                outIndex = kInvalidIndex;
                return true;
            }

            const rapidjson::Value& texture = parentValue[name];

            if (!texture.IsObject() ||
                !texture.HasMember("index") || !texture["index"].IsUint())
            {
                LogError("%s: Material texture has missing/invalid properties", mPath.GetCString());
                return false;
            }

            if (texture.HasMember("texCoord"))
            {
                LogError("%s: Multiple texture coordinates are unsupported", mPath.GetCString());
                return false;
            }
            else if (texture.HasMember("strength"))
            {
                LogError("%s: Occlusion texture strength is unsupported", mPath.GetCString());
                return false;
            }

            outIndex = texture["index"].GetUint();

            if (outIndex >= mTextures.size())
            {
                LogError("%s: Texture %u does not exist", mPath.GetCString(), outIndex);
                return false;
            }

            return true;
        };

        if (!GetTexture(pbr,   "baseColorTexture",         material.baseColourTexture))        return false;
        if (!GetTexture(entry, "emissiveTexture",          material.emissiveTexture))          return false;
        if (!GetTexture(pbr,   "metallicRoughnessTexture", material.metallicRoughnessTexture)) return false;
        if (!GetTexture(entry, "normalTexture",            material.normalTexture))            return false;
        if (!GetTexture(entry, "occlusionTexture",         material.occlusionTexture))         return false;

        material.baseColourFactor = GetVec4 (pbr,   "baseColorFactor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        material.emissiveFactor   = GetVec3 (entry, "emissiveFactor",  glm::vec3(0.0f, 0.0f, 0.0f));
        material.metallicFactor   = GetFloat(pbr,   "metallicFactor",  1.0f);
        material.roughnessFactor  = GetFloat(pbr,   "roughnessFactor", 1.0f);
    }

    return true;
}

bool GLTFImporter::LoadMeshes()
{
    if (!mDocument.HasMember("meshes"))
    {
        return true;
    }

    const rapidjson::Value& meshes = mDocument["meshes"];

    if (!meshes.IsArray())
    {
        LogError("%s: 'meshes' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : meshes.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("primitives") || !entry["primitives"].IsArray())
        {
            LogError("%s: Mesh has missing/invalid properties", mPath.GetCString());
            return false;
        }

        MeshDef& mesh = mMeshes.emplace_back();

        const rapidjson::Value& primitives = entry["primitives"];

        for (const rapidjson::Value& primEntry : primitives.GetArray())
        {
            if (!primEntry.IsObject() ||
                !primEntry.HasMember("attributes") || !primEntry["attributes"].IsObject() ||
                (primEntry.HasMember("indices")    && !primEntry["indices"].IsUint()) ||
                (primEntry.HasMember("material")   && !primEntry["material"].IsUint()) ||
                (primEntry.HasMember("mode")       && !primEntry["mode"].IsUint()))
            {
                LogError("%s: Mesh primitive has missing/invalid properties", mPath.GetCString());
                return false;
            }

            Primitive& primitive = mesh.primitives.emplace_back();

            primitive.indices = (primEntry.HasMember("indices")) ? primEntry["indices"].GetUint() : kInvalidIndex;

            if (primitive.indices != kInvalidIndex && primitive.indices >= mAccessors.size())
            {
                LogError("%s: Accessor %u does not exist", mPath.GetCString(), primitive.indices);
                return false;
            }

            primitive.material = (primEntry.HasMember("material")) ? primEntry["material"].GetUint() : kInvalidIndex;

            if (primitive.material != kInvalidIndex && primitive.material >= mMaterials.size())
            {
                LogError("%s: Material %u does not exist", mPath.GetCString(), primitive.material);
                return false;
            }

            if (primEntry.HasMember("mode"))
            {
                const uint32_t mode = primEntry["mode"].GetUint();

                switch (mode)
                {
                    case GL_POINTS:         primitive.topology = kGPUPrimitiveTopology_PointList;       break;
                    case GL_LINES:          primitive.topology = kGPUPrimitiveTopology_LineList;        break;
                    case GL_LINE_STRIP:     primitive.topology = kGPUPrimitiveTopology_LineStrip;       break;
                    case GL_TRIANGLES:      primitive.topology = kGPUPrimitiveTopology_TriangleList;    break;
                    case GL_TRIANGLE_STRIP: primitive.topology = kGPUPrimitiveTopology_TriangleStrip;   break;
                    case GL_TRIANGLE_FAN:   primitive.topology = kGPUPrimitiveTopology_TriangleFan;     break;

                    default:
                        LogError("%s: Mesh primitive has unknown mode %u", mPath.GetCString(), mode);
                        return false;

                }
            }
            else
            {
                primitive.topology = kGPUPrimitiveTopology_TriangleList;
            }

            const rapidjson::Value& attributes = primEntry["attributes"];

            for (const auto& attribEntry : attributes.GetObject())
            {
                if (!attribEntry.value.IsUint())
                {
                    LogError("%s: Mesh primitive attribute is invalid", mPath.GetCString());
                    return false;
                }

                Attribute& attribute = primitive.attributes.emplace_back();

                attribute.accessor = attribEntry.value.GetUint();

                if (attribute.accessor >= mAccessors.size())
                {
                    LogError("%s: Accessor %u does not exist", mPath.GetCString(), attribute.accessor);
                    return false;
                }

                const char* const name = attribEntry.name.GetString();

                if (strcmp(name, "POSITION") == 0)
                {
                    attribute.semantic      = kGPUAttributeSemantic_Position;
                    attribute.semanticIndex = 0;
                }
                else if (strcmp(name, "NORMAL") == 0)
                {
                    attribute.semantic      = kGPUAttributeSemantic_Normal;
                    attribute.semanticIndex = 0;
                }
                else if (strcmp(name, "TANGENT") == 0)
                {
                    attribute.semantic      = kGPUAttributeSemantic_Tangent;
                    attribute.semanticIndex = 0;
                }
                else if (strcmp(name, "TEXCOORD_0") == 0)
                {
                    attribute.semantic      = kGPUAttributeSemantic_TexCoord;
                    attribute.semanticIndex = 0;
                }
                else if (strcmp(name, "TEXCOORD_1") == 0)
                {
                    attribute.semantic      = kGPUAttributeSemantic_TexCoord;
                    attribute.semanticIndex = 1;
                }
                else if (strcmp(name, "COLOR_0") == 0)
                {
                    attribute.semantic      = kGPUAttributeSemantic_Colour;
                    attribute.semanticIndex = 0;
                }
                else
                {
                    LogError("%s: Mesh primitive attribute has unhandled semantic '%s'", mPath.GetCString(), name);
                    return false;
                }
            }
        }
    }

    return true;
}

bool GLTFImporter::LoadNodes()
{
    if (!mDocument.HasMember("nodes"))
    {
        return true;
    }

    const rapidjson::Value& nodes = mDocument["nodes"];

    if (!nodes.IsArray())
    {
        LogError("%s: 'nodes' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : nodes.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("mesh")        || !entry["mesh"].IsUint() ||
            (entry.HasMember("translation") && (!entry["translation"].IsArray() || entry["translation"].Size() != 3)) ||
            (entry.HasMember("scale")       && (!entry["scale"].IsArray()       || entry["scale"].Size() != 3)) ||
            (entry.HasMember("rotation")    && (!entry["rotation"].IsArray()    || entry["rotation"].Size() != 4)))
        {
            LogError("%s: Node has missing/invalid properties", mPath.GetCString());
            return false;
        }

        Node& node = mNodes.emplace_back();

        // TODO: Mesh is optional, but I think this is for animation which we
        // don't support yet.
        node.mesh = entry["mesh"].GetUint();

        if (node.mesh >= mMeshes.size())
        {
            LogError("%s: Mesh %u does not exist", mPath.GetCString(), node.mesh);
            return false;
        }

        node.translation = GetVec3(entry, "translation", glm::vec3(0.0f, 0.0f, 0.0f));
        node.scale       = GetVec3(entry, "scale",       glm::vec3(1.0f, 1.0f, 1.0f));
        node.rotation    = GetQuat(entry, "rotation",    glm::quat(0.0f, 0.0f, 0.0f, 1.0f));
    }

    return true;
}

bool GLTFImporter::LoadSamplers()
{
    if (!mDocument.HasMember("samplers"))
    {
        return true;
    }

    const rapidjson::Value& samplers = mDocument["samplers"];

    if (!samplers.IsArray())
    {
        LogError("%s: 'samplers' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : samplers.GetArray())
    {
        if (!entry.IsObject() ||
            (entry.HasMember("magFilter") && !entry["magFilter"].IsUint()) ||
            (entry.HasMember("minFilter") && !entry["minFilter"].IsUint()) ||
            (entry.HasMember("wrapS")     && !entry["wrapS"].IsUint()) ||
            (entry.HasMember("wrapT")     && !entry["wrapT"].IsUint()))
        {
            LogError("%s: Sampler has missing/invalid properties", mPath.GetCString());
            return false;
        }

        Sampler& sampler = mSamplers.emplace_back();

        /* We ignore filtering settings at the moment, it is intended that
         * in future texture filtering behaviour should be driven by engine
         * settings. TextureLoader will just always use trilinear filtering and
         * generate mipmaps right now. Not sure if there's ever going to be
         * any case where we would actually want the glTF settings to override
         * our settings. */
        if (entry.HasMember("magFilter"))
        {
            LogWarning("%s: Sampler filter settings are currently ignored", mPath.GetCString());
        }

        if (entry.HasMember("minFilter"))
        {
            LogWarning("%s: Sampler filter settings are currently ignored", mPath.GetCString());
        }

        auto ConvertWrapMode = [&] (const uint32_t  wrapMode,
                                    GPUAddressMode& outMode) -> bool
        {
            switch (wrapMode)
            {
                case GL_REPEAT:          outMode = kGPUAddressMode_Repeat;  return true;
                case GL_CLAMP_TO_EDGE:   outMode = kGPUAddressMode_Clamp; return true;
                case GL_MIRRORED_REPEAT: outMode = kGPUAddressMode_MirroredRepeat; return true;

                default:
                    LogError("%s: Sampler has unhandled wrap mode %u", mPath.GetCString(), wrapMode);
                    return false;
            }
        };

        if (entry.HasMember("wrapS"))
        {
            if (!ConvertWrapMode(entry["wrapS"].GetUint(), sampler.wrapS))
            {
                return false;
            }
        }

        if (entry.HasMember("wrapT"))
        {
            if (!ConvertWrapMode(entry["wrapT"].GetUint(), sampler.wrapT))
            {
                return false;
            }
        }
    }

    return true;
}

bool GLTFImporter::LoadScene()
{
    /*
     * Can have multiple scenes in a file with "scene" specifying the default
     * scene. We'll only load that one.
     */

    if (!mDocument.HasMember("scene"))
    {
        LogError("%s: Missing scene, nothing to import", mPath.GetCString());
        return false;
    }
    else if (!mDocument["scene"].IsUint())
    {
        LogError("%s: 'scene' property is invalid", mPath.GetCString());
        return false;
    }
    else if (!mDocument.HasMember("scenes") || !mDocument["scenes"].IsArray())
    {
        LogError("%s: 'scenes' property is missing/invalid", mPath.GetCString());
        return false;
    }

    const uint32_t sceneIndex = mDocument["scene"].GetUint();

    if (sceneIndex >= mDocument["scenes"].Size())
    {
        LogError("%s: Scene %u does not exist", mPath.GetCString(), sceneIndex);
        return false;
    }

    const rapidjson::Value& entry = mDocument["scenes"][sceneIndex];

    if (!entry.IsObject() ||
        !entry.HasMember("nodes") || !entry["nodes"].IsArray())
    {
        LogError("%s: Scene has missing/invalid properties", mPath.GetCString());
        return false;
    }

    const rapidjson::Value& nodes = entry["nodes"];

    for (const rapidjson::Value& node : nodes.GetArray())
    {
        if (!node.IsUint())
        {
            LogError("%s: Scene node is invalid", mPath.GetCString());
            return false;
        }

        mScene.emplace_back(node.GetUint());
    }

    if (mScene.empty())
    {
        LogError("%s: Empty scene, nothing to import", mPath.GetCString());
        return false;
    }

    return true;
}

bool GLTFImporter::LoadTextures()
{
    if (!mDocument.HasMember("textures"))
    {
        return true;
    }

    const rapidjson::Value& textures = mDocument["textures"];

    if (!textures.IsArray())
    {
        LogError("%s: 'textures' property is invalid", mPath.GetCString());
        return false;
    }

    for (const rapidjson::Value& entry : textures.GetArray())
    {
        if (!entry.IsObject() ||
            !entry.HasMember("source")  || !entry["source"].IsUint() ||
            (entry.HasMember("sampler") && !entry["sampler"].IsUint()))
        {
            LogError("%s: Texture has missing/invalid properties", mPath.GetCString());
            return false;
        }

        TextureDef& texture = mTextures.emplace_back();

        texture.image = entry["source"].GetUint();

        if (texture.image >= mImages.size())
        {
            LogError("%s: Image %u does not exist", mPath.GetCString(), texture.image);
            return false;
        }

        if (entry.HasMember("sampler"))
        {
            texture.sampler = entry["sampler"].GetUint();

            if (texture.sampler >= mSamplers.size())
            {
                LogError("%s: Sampler %u does not exist", mPath.GetCString(), texture.sampler);
                return false;
            }
        }
        else
        {
            texture.sampler = kInvalidIndex;
        }
    }

    return true;
}

bool GLTFImporter::LoadURI(const rapidjson::Value& uriValue,
                           ByteArray&              outData,
                           std::string&            outMediaType)
{
    if (!uriValue.IsString())
    {
        LogError("%s: 'uri' is invalid", mPath.GetCString());
        return false;
    }

    const char* uri = uriValue.GetString();

    static constexpr char kDataURIPrefix[]   = "data:";
    static constexpr char kBase64Specifier[] = ";base64,";

    if (strncmp(uri, kDataURIPrefix, strlen(kDataURIPrefix)) == 0)
    {
        uri += strlen(kDataURIPrefix);

        size_t mediaTypeLen = 0;
        while (uri[mediaTypeLen] &&
               uri[mediaTypeLen] != ';' &&
               uri[mediaTypeLen] != ',')
        {
            mediaTypeLen++;
        }

        if (!uri[mediaTypeLen])
        {
            LogError("%s: URI is malformed", mPath.GetCString());
            return false;
        }

        outMediaType = std::string(uri, mediaTypeLen);
        uri += mediaTypeLen;

        if (strncmp(uri, kBase64Specifier, strlen(kBase64Specifier)) != 0)
        {
            LogError("%s: Non-base64 data URI is unhandled", mPath.GetCString());
            return false;
        }

        uri += strlen(kBase64Specifier);

        if (!Base64::Decode(uri, strlen(uri), outData))
        {
            LogError("%s: URI has malformed base64 data", mPath.GetCString());
            return false;
        }
    }
    else
    {
        const Path path = mPath.GetDirectoryName() / uri;

        UPtr<DataStream> file(Filesystem::OpenFile(path));
        if (!file)
        {
            LogError("%s: Failed to open URI '%s' ('%s')", mPath.GetCString(), uri, path.GetCString());
            return false;
        }

        outData = ByteArray(file->GetSize());
        if (!file->Read(outData.Get(), outData.GetSize()))
        {
            LogError("%s: Failed to read URI '%s'", mPath.GetCString(), uri);
            return false;
        }

        /* Guess type from extension. */
        const std::string extension = path.GetExtension();
        if (extension == "jpg")
        {
            outMediaType = "image/jpeg";
        }
        else if (extension == "png")
        {
            outMediaType = "image/png";
        }
    }

    return true;
}

bool GLTFImporter::GenerateMaterial(const uint32_t materialIndex)
{
    MaterialDef& material = mMaterials[materialIndex];

    if (material.asset)
    {
        return true;
    }

    ShaderTechniquePtr shaderTechnique = AssetManager::Get().Load<ShaderTechnique>("Engine/Techniques/PBRMetallicRoughness");
    if (!shaderTechnique)
    {
        return false;
    }

    MaterialPtr asset(new Material(shaderTechnique));
    material.asset = asset;

    auto SetTexture = [&] (const char* const name,
                           const uint32_t    index,
                           const bool        sRGB)
    {
        if (index == kInvalidIndex)
        {
            /* Leave as the material default. */
            return true;
        }

        if (!GenerateTexture(index, sRGB))
        {
            return false;
        }

        asset->SetArgument(name, mTextures[index].asset);
        return true;
    };

    /* Base colour and emissive are in sRGB space. */
    if (!SetTexture("baseColourTexture",        material.baseColourTexture,        true))  return false;
    if (!SetTexture("emissiveTexture",          material.emissiveTexture,          true))  return false;
    if (!SetTexture("metallicRoughnessTexture", material.metallicRoughnessTexture, false)) return false;
    if (!SetTexture("normalTexture",            material.normalTexture,            false)) return false;
    if (!SetTexture("occlusionTexture",         material.occlusionTexture,         false)) return false;

    asset->SetArgument("baseColourFactor", material.baseColourFactor);
    asset->SetArgument("emissiveFactor",   material.emissiveFactor);
    asset->SetArgument("metallicFactor",   material.metallicFactor);
    asset->SetArgument("roughnessFactor",  material.roughnessFactor);

    asset->UpdateArgumentSet();

    const Path assetPath = mAssetDir / StringUtils::Format("Material_%u", materialIndex);
    if (!AssetManager::Get().SaveAsset(asset, assetPath))
    {
        LogError("%s: Failed to save Material asset", mPath.GetCString());
        return false;
    }

    return true;
}

bool GLTFImporter::GenerateMesh(const uint32_t meshIndex)
{
    MeshDef& mesh = mMeshes[meshIndex];

    /* We create each primitive as a separate mesh asset. This is because each
     * primitive has a separate set of vertex data in glTF, whereas the Mesh
     * asset uses shared vertex data with just separate indices/material for
     * each submesh. */
    for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
    {
        Primitive& primitive = mesh.primitives[primitiveIndex];

        if (primitive.asset)
        {
            continue;
        }

        MeshPtr asset(new Mesh());
        primitive.asset = asset;

        uint32_t vertexCount = 0;
        GPUVertexInputStateDesc inputDesc;

        /* We map attributes sharing the same buffer view onto a single
         * buffer in the mesh. This stores the mapping of buffer view index
         * to mesh buffer slot (+ 1, 0 == not seen yet). */
        uint8_t* bufferViewMapping = AllocateZeroedStackArray(uint8_t, mBufferViews.size());
        uint8_t bufferCount = 0;

        /* Build vertex input state. */
        for (size_t attributeIndex = 0; attributeIndex < primitive.attributes.size(); attributeIndex++)
        {
            const Attribute& attribute = primitive.attributes[attributeIndex];
            const Accessor& accessor   = mAccessors[attribute.accessor];

            if (vertexCount == 0)
            {
                vertexCount = accessor.count;
            }
            else
            {
                /* Expect all accessors to have the same count. */
                if (accessor.count != vertexCount)
                {
                    LogError("%s: Mesh attribute accessors have mismatching counts (%u / %u)",
                             mPath.GetCString(),
                             vertexCount,
                             accessor.count);
                    return false;
                }
            }

            if (bufferViewMapping[accessor.bufferView] == 0)
            {
                uint8_t bufferIndex = bufferCount++;
                bufferViewMapping[accessor.bufferView] = bufferIndex + 1;

                const BufferView& bufferView = mBufferViews[accessor.bufferView];

                if (bufferView.stride != 0)
                {
                    inputDesc.buffers[bufferIndex].stride = bufferView.stride;
                }
                else
                {
                    /* Stride is defined by the width of the accessor type. */
                    inputDesc.buffers[bufferIndex].stride = GPUUtils::GetAttributeSize(accessor.format);
                }
            }

            inputDesc.attributes[attributeIndex].semantic = attribute.semantic;
            inputDesc.attributes[attributeIndex].index    = attribute.semanticIndex;
            inputDesc.attributes[attributeIndex].format   = accessor.format;
            inputDesc.attributes[attributeIndex].buffer   = bufferViewMapping[accessor.bufferView] - 1;

            /* Vertex buffer content will just get everything from the start of
             * the view. */
            inputDesc.attributes[attributeIndex].offset = accessor.offset;
        }

        asset->SetVertexLayout(inputDesc, vertexCount);

        /* Set vertex data. */
        for (size_t bufferViewIndex = 0; bufferViewIndex < mBufferViews.size(); bufferViewIndex++)
        {
            if (bufferViewMapping[bufferViewIndex] != 0)
            {
                const uint8_t bufferIndex    = bufferViewMapping[bufferViewIndex] - 1;
                const BufferView& bufferView = mBufferViews[bufferViewIndex];
                const size_t requiredSize    = inputDesc.buffers[bufferIndex].stride * vertexCount;

                if (requiredSize > bufferView.length)
                {
                    LogError("%s: Buffer view %u does not have enough data (expect at least %zu)",
                             mPath.GetCString(),
                             bufferViewIndex,
                             requiredSize);
                    return false;
                }

                asset->SetVertexData(bufferIndex,
                                     mBuffers[bufferView.buffer].Get() + bufferView.offset);
            }
        }

        const uint32_t materialIndex = asset->AddMaterial("Material");

        if (primitive.indices != kInvalidIndex)
        {
            const Accessor& accessor     = mAccessors[primitive.indices];
            const BufferView& bufferView = mBufferViews[accessor.bufferView];

            GPUIndexType indexType;
            switch (accessor.format)
            {
                case kGPUAttributeFormat_R16_UInt:
                    indexType = kGPUIndexType_16;
                    break;

                default:
                    LogError("%s: Accessor %u has unhandled index format", mPath.GetCString(), primitive.indices);
                    return false;

            }

            const size_t requiredSize = accessor.offset + (accessor.count * GPUUtils::GetIndexSize(indexType));

            if (requiredSize > bufferView.length)
            {
                LogError("%s: Buffer view %u does not have enough data (expect at least %zu)",
                         mPath.GetCString(),
                         accessor.bufferView,
                         requiredSize);
                return false;
            }

            asset->AddIndexedSubMesh(materialIndex,
                                     primitive.topology,
                                     accessor.count,
                                     indexType,
                                     mBuffers[bufferView.buffer].Get() + bufferView.offset + accessor.offset);
        }
        else
        {
            asset->AddSubMesh(materialIndex,
                              primitive.topology,
                              0,
                              vertexCount);
        }

        /* Save the mesh asset. */
        const Path assetPath = mAssetDir / StringUtils::Format("Mesh_%u_Primitive_%u", meshIndex, primitiveIndex);
        if (!AssetManager::Get().SaveAsset(asset, assetPath))
        {
            LogError("%s: Failed to save Mesh asset", mPath.GetCString());
            return false;
        }

        /* Build it. Currently, we must serialise before this. */
        asset->Build();
    }

    return true;
}

bool GLTFImporter::GenerateScene()
{
    for (const uint32_t nodeIndex : mScene)
    {
        const Node& node = mNodes[nodeIndex];

        /* Generates the meshes if they haven't already been. */
        if (!GenerateMesh(node.mesh))
        {
            return false;
        }

        const MeshDef& mesh = mMeshes[node.mesh];

        // TODO: Use names from the glTF if they're there?
        std::string entityName = StringUtils::Format("%s_%u", mPath.GetBaseFileName().c_str(), nodeIndex);

        /* If we have just one primitive, we'll create it under the root, else
         * we'll create a parent entity to maintain the transform, and then put
         * each primitive as a child of that. */
        Entity* const entity = mWorld->CreateEntity(std::move(entityName));
        entity->SetTransform(Transform(node.translation, node.rotation, node.scale));

        for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            Entity* primEntity;
            if (mesh.primitives.size() > 1)
            {
                const std::string primName = StringUtils::Format("Primitive_%u", primitiveIndex);
                primEntity = entity->CreateChild(std::move(primName));
                primEntity->SetActive(true);
            }
            else
            {
                primEntity = entity;
            }

            const Primitive& primitive = mesh.primitives[primitiveIndex];

            /* Generates the material if not already done. */
            if (!GenerateMaterial(primitive.material))
            {
                return false;
            }

            const MaterialDef& material = mMaterials[primitive.material];

            MeshRenderer* const meshRenderer = primEntity->CreateComponent<MeshRenderer>();
            meshRenderer->SetMesh(primitive.asset);
            meshRenderer->SetMaterial(0, material.asset);
            meshRenderer->SetActive(true);
        }

        entity->SetActive(true);
    }

    return true;
}

bool GLTFImporter::GenerateTexture(const uint32_t textureIndex,
                                   const bool     sRGB)
{
    TextureDef& texture = mTextures[textureIndex];

    if (texture.asset)
    {
        Assert(sRGB == texture.sRGB);
        return true;
    }

    const Image& image = mImages[texture.image];

    const Sampler sampler = (texture.sampler != kInvalidIndex)
                                ? mSamplers[texture.sampler]
                                : Sampler();

    /* We'll just save out the image data into the asset filesystem, since it's
     * either JPEG or PNG and we can load those directly. */
    const Path assetPath = mAssetDir / StringUtils::Format("Texture_%u", textureIndex);

    Path baseFSPath;
    if (!AssetManager::Get().GetFilesystemPath(assetPath, baseFSPath))
    {
        LogError("%s: Failed to map asset path '%s'", mPath.GetCString(), assetPath.GetCString());
        return false;
    }

    /* Write the main texture data. */
    const char* loaderClass = nullptr;
    {
        Path fsPath = baseFSPath;
        switch (image.type)
        {
            case kImageType_JPG:
                fsPath += ".jpg";
                loaderClass = "JPGLoader";
                break;

            case kImageType_PNG:
                fsPath += ".png";
                loaderClass = "PNGLoader";
                break;
        }

        UPtr<File> file(Filesystem::OpenFile(fsPath, kFileMode_Write | kFileMode_Create | kFileMode_Truncate));
        if (!file)
        {
            LogError("%s: Failed to open '%s'", mPath.GetCString(), fsPath.GetCString());
            return false;
        }

        if (!file->Write(image.data.Get(), image.data.GetSize()))
        {
            LogError("%s: Failed to write '%s'", mPath.GetCString(), fsPath.GetCString());
            return false;
        }
    }

    /* Write loader metadata specifying properties. TODO: Better interface for
     * doing this with proper serialisation. */
    {
        const std::string loaderString = StringUtils::Format(
            "[\n"
            "   {\n"
            "       \"objectClass\": \"%s\",\n"
            "       \"objectID\": 0,\n"
            "       \"objectProperties\": {\n"
            "           \"addressU\": \"%s\",\n"
            "           \"addressV\": \"%s\",\n"
            "           \"sRGB\": %s\n"
            "       }\n"
            "   }\n"
            "]\n",
            loaderClass,
            MetaType::Lookup<GPUAddressMode>().GetEnumConstantName(sampler.wrapS),
            MetaType::Lookup<GPUAddressMode>().GetEnumConstantName(sampler.wrapT),
            (sRGB) ? "true" : "false");

        Path fsPath = baseFSPath + ".loader";

        UPtr<File> file(Filesystem::OpenFile(fsPath, kFileMode_Write | kFileMode_Create | kFileMode_Truncate));
        if (!file)
        {
            LogError("%s: Failed to open '%s'", mPath.GetCString(), fsPath.GetCString());
            return false;
        }

        if (!file->Write(loaderString.c_str(), loaderString.length()))
        {
            LogError("%s: Failed to write '%s'", mPath.GetCString(), fsPath.GetCString());
            return false;
        }
    }

    /* Now load it back in as a proper texture asset. */
    texture.asset = AssetManager::Get().Load<Texture2D>(assetPath);
    texture.sRGB  = sRGB;

    return texture.asset != nullptr;
}
