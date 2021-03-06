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

#include "Engine/Texture.h"

#include "Render/ShaderTechnique.h"

/**
 * A material is a combination of a shader technique, and argument values for
 * that technique's parameters.
 */
class Material : public Asset
{
    CLASS();

public:
                                Material(ShaderTechnique* const               shaderTechnique,
                                         const ShaderTechnique::FeatureArray& features = {});

    ShaderTechnique*            GetShaderTechnique() const  { return mShaderTechnique; }
    uint32_t                    GetFeatures() const         { return mFeatures; }
    GPUArgumentSet*             GetArgumentSet() const      { return mArgumentSet; }
    bool                        HasConstants() const        { return mConstantData.GetSize() > 0; }

    const ShaderVariant*        GetVariant(const ShaderPassType passType) const
                                    { return mVariants[passType]; }

    void                        GetArgument(const std::string&        name,
                                            const ShaderParameterType type,
                                            void* const               outData) const;

    template <typename T>
    void                        GetArgument(const std::string& name,
                                            T&                 outValue) const;

    void                        SetArgument(const std::string&        name,
                                            const ShaderParameterType type,
                                            const void* const         data);

    template <typename T>
    void                        SetArgument(const std::string& name,
                                            const T&           value);

    /** Get GPU constants based on current argument values. */
    GPUConstants                GetGPUConstants();

private:
                                Material();
                                ~Material();

    void                        Serialise(Serialiser& serialiser) const override;
    void                        Deserialise(Serialiser& serialiser) override;

    void                        SetShaderTechnique(ShaderTechnique* const shaderTechnique,
                                                   const uint32_t         features,
                                                   const bool             createArguments);

    void                        GetArgument(const ShaderParameter& parameter,
                                            void* const            outData) const;

    void                        SetArgument(const ShaderParameter& parameter,
                                            const void* const      data);

    void                        UpdateArgumentSet();

private:
    ShaderTechniquePtr          mShaderTechnique;
    uint32_t                    mFeatures;

    const ShaderVariant*        mVariants[kShaderPassTypeCount];

    GPUArgumentSet*             mArgumentSet;

    /**
     * Array of resources, indexed by the parameter's argument index. This may
     * waste a bit of memory since we don't actually store anything in the
     * array entries corresponding to sampler arguments (the sampler comes from
     * the main TextureBase), but doing things this way is simpler.
     */
    std::vector<ObjPtr<>>       mResources;

    /**
     * Constant buffer data, laid out according to the technique's parameter
     * specification.
     */
    ByteArray                   mConstantData;

    /** Current GPU constant data. Copied on first use in a frame. */
    GPUConstants                mGPUConstants;
    uint64_t                    mGPUConstantsFrameIndex;

};

using MaterialPtr = ObjPtr<Material>;

template <typename T>
inline void Material::GetArgument(const std::string& name,
                                  T&                 outValue) const
{
    GetArgument(name,
                ShaderParameterTypeTraits<T>::kType,
                std::addressof(outValue));
}

template <typename T>
inline void Material::SetArgument(const std::string& name,
                                  const T&           value)
{
    SetArgument(name,
                ShaderParameterTypeTraits<T>::kType,
                std::addressof(value));
}
