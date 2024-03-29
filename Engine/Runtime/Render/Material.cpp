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

#include "Render/Material.h"

#include "Core/Thread.h"

#include "Engine/Engine.h"
#include "Engine/Serialiser.h"

#include "GPU/GPUConstantPool.h"
#include "GPU/GPUDevice.h"

Material::Material() :
    mVariants               {},
    mArgumentSet            (nullptr),
    mGPUConstants           (kGPUConstants_Invalid),
    mGPUConstantsFrameIndex (0)
{
}

Material::Material(ShaderTechnique* const               shaderTechnique,
                   const ShaderTechnique::FeatureArray& features) :
    Material ()
{
    const uint32_t mask = shaderTechnique->ConvertFeatureArray(features);

    /* Create initial argument set from defaults when we create a material at
     * runtime. */
    SetShaderTechnique(shaderTechnique, mask, true);
}

Material::~Material()
{
    delete mArgumentSet;
}

void Material::Serialise(Serialiser& serialiser) const
{
    Asset::Serialise(serialiser);

    serialiser.Write("shaderTechnique", mShaderTechnique);

    serialiser.BeginArray("features");

    const ShaderTechnique::FeatureArray& features = mShaderTechnique->GetFeatures();

    for (uint32_t i = 0; i < features.size(); i++)
    {
        if (mFeatures & (1 << i))
        {
            serialiser.Push(features[i]);
        }
    }

    serialiser.EndArray();

    serialiser.BeginGroup("arguments");

    for (const ShaderParameter& parameter : mShaderTechnique->GetParameters())
    {
        if ((mFeatures & parameter.requires) == parameter.requires)
        {
            #define WRITE_TYPE(typeEnum, typeName) \
                case typeEnum: \
                { \
                    typeName value; \
                    GetArgument(parameter, &value); \
                    serialiser.Write(parameter.name.c_str(), value); \
                    break; \
                }

            switch (parameter.type)
            {
                WRITE_TYPE(kShaderParameterType_Int,         int32_t);
                WRITE_TYPE(kShaderParameterType_Int2,        glm::ivec2);
                WRITE_TYPE(kShaderParameterType_Int3,        glm::ivec3);
                WRITE_TYPE(kShaderParameterType_Int4,        glm::ivec4);
                WRITE_TYPE(kShaderParameterType_UInt,        uint32_t);
                WRITE_TYPE(kShaderParameterType_UInt2,       glm::uvec2);
                WRITE_TYPE(kShaderParameterType_UInt3,       glm::uvec3);
                WRITE_TYPE(kShaderParameterType_UInt4,       glm::uvec4);
                WRITE_TYPE(kShaderParameterType_Float,       float);
                WRITE_TYPE(kShaderParameterType_Float2,      glm::vec2);
                WRITE_TYPE(kShaderParameterType_Float3,      glm::vec3);
                WRITE_TYPE(kShaderParameterType_Float4,      glm::vec4);
                WRITE_TYPE(kShaderParameterType_Texture2D,   Texture2DPtr);
                WRITE_TYPE(kShaderParameterType_TextureCube, TextureCubePtr);

                default:
                    UnreachableMsg("Unhandled parameter type %d", parameter.type);
                    break;

            }

            #undef WRITE_TYPE
        }
    }

    serialiser.EndGroup();
}

void Material::Deserialise(Serialiser& serialiser)
{
    Asset::Deserialise(serialiser);

    bool found;

    ShaderTechniquePtr shaderTechnique;
    found = serialiser.Read("shaderTechnique", shaderTechnique);
    Assert(found);

    const uint32_t features = shaderTechnique->DeserialiseFeatureArray(serialiser, "features");

    /* Defer argument set creation until the end so we don't end up recreating
     * the set for each argument we deserialise. */
    SetShaderTechnique(shaderTechnique, features, false);

    if (serialiser.BeginGroup("arguments"))
    {
        for (const ShaderParameter& parameter : mShaderTechnique->GetParameters())
        {
            if ((mFeatures & parameter.requires) == parameter.requires)
            {
                #define READ_TYPE(typeEnum, typeName) \
                    case typeEnum: \
                    { \
                        typeName value; \
                        if (serialiser.Read(parameter.name.c_str(), value)) \
                        { \
                            SetArgument(parameter, &value); \
                        } \
                        break; \
                    }

                switch (parameter.type)
                {
                    READ_TYPE(kShaderParameterType_Int,         int32_t);
                    READ_TYPE(kShaderParameterType_Int2,        glm::ivec2);
                    READ_TYPE(kShaderParameterType_Int3,        glm::ivec3);
                    READ_TYPE(kShaderParameterType_Int4,        glm::ivec4);
                    READ_TYPE(kShaderParameterType_UInt,        uint32_t);
                    READ_TYPE(kShaderParameterType_UInt2,       glm::uvec2);
                    READ_TYPE(kShaderParameterType_UInt3,       glm::uvec3);
                    READ_TYPE(kShaderParameterType_UInt4,       glm::uvec4);
                    READ_TYPE(kShaderParameterType_Float,       float);
                    READ_TYPE(kShaderParameterType_Float2,      glm::vec2);
                    READ_TYPE(kShaderParameterType_Float3,      glm::vec3);
                    READ_TYPE(kShaderParameterType_Float4,      glm::vec4);
                    READ_TYPE(kShaderParameterType_Texture2D,   Texture2DPtr);
                    READ_TYPE(kShaderParameterType_TextureCube, TextureCubePtr);

                    default:
                        UnreachableMsg("Unhandled parameter type %d", parameter.type);
                        break;

                }

                #undef READ_TYPE
            }
        }

        serialiser.EndGroup();
    }

    UpdateArgumentSet();
}

void Material::SetShaderTechnique(ShaderTechnique* const shaderTechnique,
                                  const uint32_t         features,
                                  const bool             createArguments)
{
    /* If ever we want to support changing technique at runtime, we'd have to
     * propagate this change all the way back to all entities using the material
     * to recreate their pipelines. */
    Assert(!mShaderTechnique);

    mShaderTechnique = shaderTechnique;
    mFeatures        = features;

    mResources = mShaderTechnique->GetDefaultResources();

    mConstantData = ByteArray(mShaderTechnique->GetConstantsSize());
    memcpy(mConstantData.Get(),
           mShaderTechnique->GetDefaultConstantData().Get(),
           mConstantData.GetSize());

    if (createArguments)
    {
        UpdateArgumentSet();
    }

    for (uint32_t passType = 0; passType < kShaderPassTypeCount; passType++)
    {
        mVariants[passType] = mShaderTechnique->GetVariant(static_cast<ShaderPassType>(passType), mFeatures);
    }
}

void Material::GetArgument(const ShaderParameter& parameter,
                           void* const            outData) const
{
    if (ShaderParameter::IsConstant(parameter.type))
    {
        memcpy(outData,
               mConstantData.Get() + parameter.constantOffset,
               ShaderParameter::GetSize(parameter.type));
    }
    else
    {
        const ObjPtr<>& resource = mResources[parameter.argumentIndex];

        switch (parameter.type)
        {
            case kShaderParameterType_Texture2D:
            {
                auto texture = reinterpret_cast<Texture2DPtr*>(outData);
                *texture = static_cast<Texture2D*>(resource.Get());
                break;
            }

            case kShaderParameterType_TextureCube:
            {
                auto texture = reinterpret_cast<TextureCubePtr*>(outData);
                *texture = static_cast<TextureCube*>(resource.Get());
                break;
            }

            default:
                UnreachableMsg("Unhandled ShaderParameterType");

        }
    }
}

void Material::GetArgument(const std::string&        name,
                           const ShaderParameterType type,
                           void* const               outData) const
{
    const ShaderParameter* const parameter = mShaderTechnique->FindParameter(name);

    AssertMsg(parameter,
              "Parameter '%s' not found in technique '%s'",
              name.c_str(),
              mShaderTechnique->GetPath().c_str());

    AssertMsg(parameter->type == type,
              "Type mismatch for parameter '%s' in technique '%s' (requested '%s', actual '%s')",
              name.c_str(),
              mShaderTechnique->GetPath().c_str(),
              MetaType::Lookup<ShaderParameterType>().GetEnumConstantName(type),
              MetaType::Lookup<ShaderParameterType>().GetEnumConstantName(parameter->type));

    AssertMsg((mFeatures & parameter->requires) == parameter->requires,
              "Parameter '%s' is not enabled by material features",
              name.c_str());

    GetArgument(*parameter, outData);
}

void Material::SetArgument(const ShaderParameter& parameter,
                           const void* const      data)
{
    if (ShaderParameter::IsConstant(parameter.type))
    {
        memcpy(mConstantData.Get() + parameter.constantOffset,
               data,
               ShaderParameter::GetSize(parameter.type));
    }
    else
    {
        ObjPtr<>& resource = mResources[parameter.argumentIndex];

        switch (parameter.type)
        {
            case kShaderParameterType_Texture2D:
            {
                auto texture = reinterpret_cast<const Texture2DPtr*>(data);
                resource = texture->Get();
                break;
            }

            case kShaderParameterType_TextureCube:
            {
                auto texture = reinterpret_cast<const TextureCubePtr*>(data);
                resource = texture->Get();
                break;
            }

            default:
                UnreachableMsg("Unhandled ShaderParameterType");

        }

        /* If the argument is set to null, we'll set it back to the default. We
         * always need valid resource arguments to be able to create argument
         * sets. */
        if (!resource)
        {
            resource = mShaderTechnique->GetDefaultResources()[parameter.argumentIndex];
        }

        /* When resources change while we have an argument set, we need to
         * recreate it. */
        if (mArgumentSet)
        {
            UpdateArgumentSet();
        }
    }
}

void Material::SetArgument(const std::string&        name,
                           const ShaderParameterType type,
                           const void* const         data)
{
    const ShaderParameter* const parameter = mShaderTechnique->FindParameter(name);

    AssertMsg(parameter,
              "Parameter '%s' not found in technique '%s'",
              name.c_str(),
              mShaderTechnique->GetPath().c_str());

    AssertMsg(parameter->type == type,
              "Type mismatch for parameter '%s' in technique '%s' (requested '%s', actual '%s')",
              name.c_str(),
              mShaderTechnique->GetPath().c_str(),
              MetaType::Lookup<ShaderParameterType>().GetEnumConstantName(type),
              MetaType::Lookup<ShaderParameterType>().GetEnumConstantName(parameter->type));

    AssertMsg((mFeatures & parameter->requires) == parameter->requires,
              "Parameter '%s' is not enabled by material features",
              name.c_str());

    SetArgument(*parameter, data);
}

GPUConstants Material::GetGPUConstants()
{
    /*
     * We'll write constants on first use in a frame, and then reuse the same
     * handle for subsequent uses in the same frame. This means we don't
     * repeatedly write constants for multiple entities using the same material.
     *
     * Any argument changes should take place before rendering (e.g. in entity
     * update), so we don't need to worry about updating to reflect changes
     * after we've created for the first time.
     *
     * TODO: Does this need to be made thread-safe? Potentially in future we
     * might build up draw lists in parallel or something. Assert for now to
     * catch this if we try to do it.
     */
    Assert(Thread::IsMain());

    const uint64_t frameIndex = Engine::Get().GetFrameIndex();

    if (mGPUConstantsFrameIndex != frameIndex)
    {
        mGPUConstantsFrameIndex = frameIndex;
        mGPUConstants           = GPUDevice::Get().GetConstantPool().Write(mConstantData.Get(),
                                                                           mConstantData.GetSize());
    }
    else
    {
        Assert(mGPUConstants != kGPUConstants_Invalid);
    }

    return mGPUConstants;
}

void Material::UpdateArgumentSet()
{
    const GPUArgumentSetLayoutRef setLayout = mShaderTechnique->GetArgumentSetLayout();

    if (setLayout)
    {
        if (mArgumentSet)
        {
            delete mArgumentSet;
        }

        auto arguments = AllocateZeroedStackArray(GPUArgument, setLayout->GetArgumentCount());

        for (uint8_t i = 0; i < setLayout->GetArgumentCount(); i++)
        {
            switch (setLayout->GetArguments()[i])
            {
                case kGPUArgumentType_Texture:
                    if (mResources[i])
                    {
                        auto texture = static_cast<TextureBase*>(mResources[i].Get());
                        arguments[i].view = texture->GetResourceView();
                    }

                    break;

                case kGPUArgumentType_Sampler:
                    /* Samplers come from the texture in the preceding index. */
                    Assert(i > 0);

                    if (mResources[i - 1])
                    {
                        auto texture = static_cast<TextureBase*>(mResources[i - 1].Get());
                        arguments[i].sampler = texture->GetSampler();
                    }

                    break;

                case kGPUArgumentType_Constants:
                    break;

                default:
                    UnreachableMsg("Unhandled GPUArgumentType");

            }
        }

        mArgumentSet = GPUDevice::Get().CreateArgumentSet(setLayout, arguments);
    }
}
