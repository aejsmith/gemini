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
#include "Core/Path.h"

#include "Engine/Mesh.h"
#include "Engine/Texture.h"

#include "Entity/Entity.h"

#include "GPU/GPUDefs.h"

#include "Render/Material.h"

#include <rapidjson/document.h>

class ByteArray;

/** glTF import behaviour flags. */
enum GLTFImporterFlags : uint32_t
{
    kGLTFImporter_None = 0,

    /**
     * Although glTF specifies that normal maps should have Y as up, some
     * models have been seen that have Y as down. This is OK if we were using
     * precomputed tangents from the model which fix the sign, but we do not
     * use them. This option can be set to flip the sign to fix these models.
     */
    kGLTFImporter_NormalMapYFlip = 1 << 0,
};

DEFINE_ENUM_BITWISE_OPS(GLTFImporterFlags);

/**
 * Class for importing glTF files. This is different to a normal asset loader:
 * asset loaders are for loading a single asset, whereas glTF files contain
 * data for potentially many different asset types and entities that need to be
 * composed together:
 *
 *  - Textures
 *  - Meshes
 *  - Materials
 *  - Entities/Components
 *
 * This class is intended to be used as a one-time process to import/convert
 * a glTF file to a set of assets/entities usable by the engine. It will create
 * and save assets for all the textures/meshes/materials in the file, and then
 * create entities in the world composing the model.
 */
class GLTFImporter
{
public:
                                    GLTFImporter();
                                    ~GLTFImporter();

    /**
     * Imports the glTF file at the given filesystem path into the world as a
     * child of the specified entity. Assets generated during the process will
     * be saved in the directory given by asset manager path assetDir.
     */
    bool                            Import(const Path&             path,
                                           const Path&             assetDir,
                                           Entity* const           entity,
                                           const GLTFImporterFlags flags = kGLTFImporter_None);

private:
    struct Accessor
    {
        uint32_t                    bufferView;
        uint64_t                    offset;
        uint32_t                    count;
        GPUAttributeFormat          format;
    };

    struct BufferView
    {
        uint32_t                    buffer;
        uint64_t                    length;
        uint64_t                    offset;
        uint64_t                    stride;
    };

    enum ImageType
    {
        kImageType_PNG,
        kImageType_JPG,
    };

    struct Image
    {
        ByteArray                   data;
        ImageType                   type;
    };

    struct MaterialDef
    {
        uint32_t                    baseColourTexture;
        uint32_t                    emissiveTexture;
        uint32_t                    metallicRoughnessTexture;
        uint32_t                    normalTexture;
        uint32_t                    occlusionTexture;

        glm::vec4                   baseColourFactor;
        glm::vec3                   emissiveFactor;
        float                       metallicFactor;
        float                       roughnessFactor;

        MaterialPtr                 asset;
    };

    struct Attribute
    {
        uint32_t                    accessor;
        GPUAttributeSemantic        semantic;
        uint8_t                     semanticIndex;
    };

    struct Primitive
    {
        std::vector<Attribute>      attributes;
        uint32_t                    indices;
        uint32_t                    material;
        GPUPrimitiveTopology        topology;

        MeshPtr                     asset;
    };

    struct MeshDef
    {
        std::vector<Primitive>      primitives;
    };

    struct Node
    {
        uint32_t                    mesh;
        uint32_t                    light;
        glm::vec3                   translation;
        glm::vec3                   scale;
        glm::quat                   rotation;
        std::vector<uint32_t>       children;
    };

    struct TextureDef
    {
        uint32_t                    image;
        uint32_t                    sampler;

        Texture2DPtr                asset;
        bool                        sRGB;
    };

    struct Sampler
    {
        GPUAddressMode              wrapS = kGPUAddressMode_Repeat;
        GPUAddressMode              wrapT = kGPUAddressMode_Repeat;
    };

    struct LightDef
    {
        LightType                   type;
        glm::vec3                   colour;
        float                       intensity;
        float                       range;
        Radians                     innerConeAngle;
        Radians                     outerConeAngle;
    };

private:
    bool                            LoadAccessors();
    bool                            LoadBuffers();
    bool                            LoadBufferViews();
    bool                            LoadImages();
    bool                            LoadLights();
    bool                            LoadMaterials();
    bool                            LoadMeshes();
    bool                            LoadNodes();
    bool                            LoadSamplers();
    bool                            LoadScene();
    bool                            LoadTextures();

    bool                            GenerateMaterial(const uint32_t materialIndex);
    bool                            GenerateMesh(const uint32_t meshIndex);
    bool                            GenerateScene();
    bool                            GenerateTexture(const uint32_t textureIndex,
                                                    const bool     sRGB);

    bool                            LoadURI(const rapidjson::Value& uriValue,
                                            ByteArray&              outData,
                                            std::string&            outMediaType);

private:
    Path                            mPath;
    Path                            mAssetDir;
    Entity*                         mEntity;
    GLTFImporterFlags               mFlags;

    rapidjson::Document             mDocument;

    std::vector<Accessor>           mAccessors;
    std::vector<ByteArray>          mBuffers;
    std::vector<BufferView>         mBufferViews;
    std::vector<Image>              mImages;
    std::vector<LightDef>           mLights;
    std::vector<MaterialDef>        mMaterials;
    std::vector<MeshDef>            mMeshes;
    std::vector<Node>               mNodes;
    std::vector<TextureDef>         mTextures;
    std::vector<Sampler>            mSamplers;

    std::vector<uint32_t>           mScene;

};