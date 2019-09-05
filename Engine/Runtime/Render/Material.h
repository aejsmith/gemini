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

#include "Render/ShaderTechnique.h"

/**
 * A material is a combination of a shader technique, and argument values for
 * that technique's parameters.
 */
class Material : public Asset
{
    CLASS();

public:
    ShaderTechnique*            GetShaderTechnique() const  { return mShaderTechnique; }
    GPUArgumentSet*             GetArgumentSet() const      { return mArgumentSet; }
    bool                        HasConstants() const        { return mConstantData.GetSize() > 0; }

    void                        GetArgument(const std::string&        inName,
                                            const ShaderParameterType inType,
                                            void* const               outData) const;

    template <typename T>
    void                        GetArgument(const std::string& inName,
                                            T&                 outValue) const;

    void                        SetArgument(const std::string&        inName,
                                            const ShaderParameterType inType,
                                            const void* const         inData);

    template <typename T>
    void                        SetArgument(const std::string& inName,
                                            const T&           inValue);

    /** Get GPU constants based on current argument values. */
    GPUConstants                GetGPUConstants();

private:
                                Material();
                                ~Material();

    void                        Serialise(Serialiser& inSerialiser) const override;
    void                        Deserialise(Serialiser& inSerialiser) override;

    void                        GetArgument(const ShaderParameter& inParameter,
                                            void* const            outData) const;

    void                        SetArgument(const ShaderParameter& inParameter,
                                            const void* const      inData);

    void                        UpdateArgumentSet();

private:
    ShaderTechniquePtr          mShaderTechnique;

    GPUArgumentSet*             mArgumentSet;

    /**
     * Constant buffer data, laid out according to the technique's parameter
     * specification.
     */
    ByteArray                   mConstantData;

    /** Current GPU constant data. Copied on first use in a frame. */
    GPUConstants                mGPUConstants;
    uint64_t                    mGPUConstantsFrameIndex;

};

using MaterialPtr = ObjectPtr<Material>;

template <typename T>
inline void Material::GetArgument(const std::string& inName,
                                  T&                 outValue) const
{
    GetArgument(inName,
                ShaderParameterTypeTraits<T>::kType,
                std::addressof(outValue));
}

template <typename T>
inline void Material::SetArgument(const std::string& inName,
                                  const T&           inValue)
{
    SetArgument(inName,
                ShaderParameterTypeTraits<T>::kType,
                std::addressof(inValue));
}
