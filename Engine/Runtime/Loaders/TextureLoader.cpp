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

#include "Loaders/TextureLoader.h"

TextureLoader::TextureLoader() :
    addressU (kGPUAddressMode_Repeat),
    addressV (kGPUAddressMode_Repeat),
    addressW (kGPUAddressMode_Repeat),
    sRGB     (true),
    mFormat  (kPixelFormat_Unknown)
{
}

TextureLoader::~TextureLoader()
{
}

GPUSamplerDesc TextureLoader::GetSamplerDesc() const
{
    // TODO: Filtering settings. This should probably be handled by some
    // global default setting and then allow overriding on a per-texture basis.
    GPUSamplerDesc samplerDesc;
    samplerDesc.magFilter     = kGPUFilter_Linear;
    samplerDesc.minFilter     = kGPUFilter_Linear;
    samplerDesc.mipmapFilter  = kGPUFilter_Linear;
    samplerDesc.addressU      = this->addressU;
    samplerDesc.addressV      = this->addressV;
    samplerDesc.addressW      = this->addressW;

    return samplerDesc;
}

PixelFormat TextureLoader::GetFinalFormat() const
{
    if (this->sRGB)
    {
        return PixelFormatInfo::GetSRGBEquivalent(mFormat);
    }
    else
    {
        return PixelFormatInfo::GetNonSRGBEquivalent(mFormat);
    }
}

Texture2DLoader::Texture2DLoader() :
    mWidth  (0),
    mHeight (0)
{
}

Texture2DLoader::~Texture2DLoader()
{
}

AssetPtr Texture2DLoader::Load()
{
    if (!LoadData())
    {
        return nullptr;
    }

    return new Texture2D(mWidth,
                         mHeight,
                         0,
                         GetFinalFormat(),
                         GetSamplerDesc(),
                         mTextureData);
}

TextureCubeLoader::TextureCubeLoader()
{
}

TextureCubeLoader::~TextureCubeLoader()
{
}

const char* TextureCubeLoader::GetExtension() const
{
    /* Our data comes from other assets. */
    return nullptr;
}

AssetPtr TextureCubeLoader::Load()
{
    Texture2DPtr textures[kGPUCubeFaceCount];
    textures[kGPUCubeFace_PositiveX] = this->positiveXFace;
    textures[kGPUCubeFace_NegativeX] = this->negativeXFace;
    textures[kGPUCubeFace_PositiveY] = this->positiveYFace;
    textures[kGPUCubeFace_NegativeY] = this->negativeYFace;
    textures[kGPUCubeFace_PositiveZ] = this->positiveZFace;
    textures[kGPUCubeFace_NegativeZ] = this->negativeZFace;

    return new TextureCube(textures, GetSamplerDesc());
}
