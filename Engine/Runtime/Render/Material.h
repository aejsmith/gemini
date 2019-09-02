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

#include "Render/ShaderTechnique.h"

/**
 * A material is a combination of a shader technique and parameter values for
 * that technique.
 */
class Material : public Asset
{
    CLASS();

public:
                                Material(ShaderTechnique* const inTechnique);

    ShaderTechnique*            GetShaderTechnique() const  { return mShaderTechnique; }

private:
                                Material();
                                ~Material();

    void                        Serialise(Serialiser& inSerialiser) const override;
    void                        Deserialise(Serialiser& inSerialiser) override;

private:
    ShaderTechniquePtr          mShaderTechnique;

};

using MaterialPtr = ObjectPtr<Material>;
