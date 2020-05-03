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

#pragma once

#include "Core/ByteArray.h"

#include "Engine/Asset.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUState.h"

#include "Render/ShaderParameter.h"

#include <map>
#include <vector>

/**
 * Implementation of a shader technique for a specific pass type. Defines the
 * shaders to use and some pipeline state. Note that we cannot create a final
 * PSO here: that is dependent on vertex input state, which is dependent on the
 * specific entity. Therefore PSO creation is managed by each entity's
 * EntityRenderer component.
 */
class ShaderPass
{
public:
    GPUShader*                      GetShader(const GPUShaderStage stage) const
                                        { return mShaders[stage]; }

    GPUBlendStateRef                GetBlendState() const           { return mBlendState; }
    GPUDepthStencilStateRef         GetDepthStencilState() const    { return mDepthStencilState; }
    GPURasterizerStateRef           GetRasterizerState() const      { return mRasterizerState; }
    GPURenderTargetStateRef         GetRenderTargetState() const    { return mRenderTargetState; }

private:
                                    ShaderPass();
                                    ~ShaderPass();

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

public:
    /**
     * Get a pass of a given type. Returns null if the technique doesn't
     * support the pass type.
     */
    const ShaderPass*               GetPass(const ShaderPassType type) const
                                        { return mPasses[type]; }

    /**
     * Return an array of parameters for the technique. Constant parameters
     * will be in this array in the order of declaration in the material
     * constant buffer.
     */
    const ParameterArray&           GetParameters() const           { return mParameters; }

    size_t                          GetParameterCount() const       { return mParameters.size(); }

    /** Get a named parameter. Returns null if doesn't exist. */
    const ShaderParameter*          FindParameter(const std::string& name) const;

    GPUArgumentSetLayoutRef         GetArgumentSetLayout() const    { return mArgumentSetLayout; }
    uint32_t                        GetConstantsSize() const        { return mConstantsSize; }
    uint32_t                        GetConstantsIndex() const       { return mConstantsIndex; }

    const std::vector<ObjPtr<>>&    GetDefaultResources() const     { return mDefaultResources; }
    const ByteArray&                GetDefaultConstantData() const  { return mDefaultConstantData; }

private:
                                    ShaderTechnique();
                                    ~ShaderTechnique();

    void                            Serialise(Serialiser& serialiser) const override;
    void                            Deserialise(Serialiser& serialiser) override;

    void                            DeserialiseParameters(Serialiser& serialiser);
    void                            DeserialisePasses(Serialiser& serialiser);

private:
    ShaderPass*                     mPasses[kShaderPassTypeCount];

    // TODO: Boolean "feature" flags, which use shader variants. Parameters can
    // be set to only be enabled when a feature is enabled.

    ParameterArray                  mParameters;

    GPUArgumentSetLayoutRef         mArgumentSetLayout;
    uint32_t                        mConstantsSize;
    uint32_t                        mConstantsIndex;

    /** Default material resources/constants (see equivalents in Material). */
    std::vector<ObjPtr<>>           mDefaultResources;
    ByteArray                       mDefaultConstantData;

};

using ShaderTechniquePtr = ObjPtr<ShaderTechnique>;
