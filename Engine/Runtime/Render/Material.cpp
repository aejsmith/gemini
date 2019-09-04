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

#include "Render/Material.h"

#include "Engine/Serialiser.h"

Material::Material()
{
}

Material::Material(ShaderTechnique* const inTechnique) :
    mShaderTechnique    (inTechnique)
{
    Assert(inTechnique);
}

Material::~Material()
{
}

void Material::Serialise(Serialiser& inSerialiser) const
{
    LogError("TODO");
}

void Material::Deserialise(Serialiser& inSerialiser)
{
    bool found;

    found = inSerialiser.Read("shaderTechnique", mShaderTechnique);
    Assert(found);
}