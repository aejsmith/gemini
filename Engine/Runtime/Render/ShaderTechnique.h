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

#include "Engine/Asset.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUState.h"

#include "Render/ShaderParameter.h"

#include <vector>

/**
 * Implementation of a shader technique for a specific pass type and material
 * feature set. Defines the shaders to use and some pipeline state. Note that
 * we cannot create a final PSO here: that is dependent on vertex input state,
 * which is dependent on the specific entity. Therefore PSO creation is managed
 * by each entity's EntityRenderer component.
 */
class ShaderVariant
{
public:
    GPUShader*                      GetShader(const GPUShaderStage stage) const
                                        { return mShaders[stage]; }

    GPUBlendStateRef                GetBlendState() const           { return mBlendState; }
    GPUDepthStencilStateRef         GetDepthStencilState() const    { return mDepthStencilState; }
    GPURasterizerStateRef           GetRasterizerState() const      { return mRasterizerState; }
    GPURenderTargetStateRef         GetRenderTargetState() const    { return mRenderTargetState; }

private:
                                    ShaderVariant() {}
                                    ~ShaderVariant() {}

    uint32_t                        mFeatures;

    GPUShaderPtr                    mShaders[kGPUShaderStage_NumGraphics];

    GPUBlendStateRef                mBlendState;
    GPUDepthStencilStateRef         mDepthStencilState;
    GPURasterizerStateRef           mRasterizerState;
    GPURenderTargetStateRef         mRenderTargetState;

    friend class ShaderTechnique;
};

/**
 * A shader technique is used to render an entity. It defines the shaders to
 * use for supported pass types (see ShaderPassType), and a set of parameters
 * controlling the appearance of the rendered entity. Parameter values are
 * supplied by materials.
 */
class ShaderTechnique : public Asset
{
    CLASS();

public:
    /** Array of parameter details. */
    using ParameterArray          = std::vector<ShaderParameter>;

    /** Array of feature names. */
    using FeatureArray            = std::vector<std::string>;

public:
    const FeatureArray&             GetFeatures() const             { return mFeatures; }

    /** Get the index of a named feature. Returns false if it doesn't exist. */
    bool                            FindFeature(const std::string& name,
                                                uint32_t&          outIndex) const;

    /**
     * Return an array of parameters for the technique. Constant parameters
     * will be in this array in the order of declaration in the material
     * constant buffer.
     */
    const ParameterArray&           GetParameters() const           { return mParameters; }

    size_t                          GetParameterCount() const       { return mParameters.size(); }

    /** Get a named parameter. Returns null if it doesn't exist. */
    const ShaderParameter*          FindParameter(const std::string& name) const;

    /**
     * Get a shader variant for a given pass type and feature set. Returns null
     * if the technique doesn't support the pass type.
     */
    const ShaderVariant*            GetVariant(const ShaderPassType passType,
                                               const uint32_t       features);

    GPUArgumentSetLayoutRef         GetArgumentSetLayout() const    { return mArgumentSetLayout; }
    uint32_t                        GetConstantsSize() const        { return mConstantsSize; }
    uint32_t                        GetConstantsIndex() const       { return mConstantsIndex; }

    const std::vector<ObjPtr<>>&    GetDefaultResources() const     { return mDefaultResources; }
    const ByteArray&                GetDefaultConstantData() const  { return mDefaultConstantData; }

    /**
     * Helper functions for converting/deserialising arrays of feature strings
     * to a feature bitmask for this technique.
     */
    uint32_t                        ConvertFeatureArray(const FeatureArray& features) const;
    uint32_t                        DeserialiseFeatureArray(Serialiser&       serialiser,
                                                            const char* const name) const;

private:
    /**
     * Properties for a variant declared in the asset. The final ShaderVariant
     * used by a material is actually a combination of the properties for all
     * variant declarations that match the material's features. We do not
     * generate all the possible combinations when loading the technique - we
     * only generate them when needed by a material, otherwise we could
     * potentially generate a bunch of shaders that aren't needed.
     */
    struct VariantProps
    {
        uint32_t                    requires = 0;
        ShaderPassFlags             flags    = kShaderPassFlags_None;
        ShaderDefineArray           defines;
    };

    struct Shader
    {
        std::string                 source;
        std::string                 function;
        uint32_t                    requires = 0;
    };

    struct Pass
    {
        Shader                      shaders[kGPUShaderStage_NumGraphics];
        std::vector<VariantProps>   variantProps;
        std::vector<ShaderVariant*> variants;
    };

private:
                                    ShaderTechnique();
                                    ~ShaderTechnique();

    void                            Serialise(Serialiser& serialiser) const override;
    void                            Deserialise(Serialiser& serialiser) override;

    void                            DeserialiseFeatures(Serialiser& serialiser);
    void                            DeserialiseParameters(Serialiser& serialiser);
    void                            DeserialisePasses(Serialiser& serialiser);

private:
    Pass*                           mPasses[kShaderPassTypeCount];

    /**
     * Features supported by the technique. Features are internally referred to
     * by a number, which is an index into this array to give the name of the
     * feature. Sets of features are represented as bitmasks, where each bit
     * index corresponds to an entry in this array.
     */
    FeatureArray                    mFeatures;

    ParameterArray                  mParameters;

    GPUArgumentSetLayoutRef         mArgumentSetLayout;
    uint32_t                        mConstantsSize;
    uint32_t                        mConstantsIndex;

    /** Default material resources/constants (see equivalents in Material). */
    std::vector<ObjPtr<>>           mDefaultResources;
    ByteArray                       mDefaultConstantData;

};

using ShaderTechniquePtr = ObjPtr<ShaderTechnique>;
