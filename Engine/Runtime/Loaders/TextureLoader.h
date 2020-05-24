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

#include "Engine/AssetLoader.h"
#include "Engine/Texture.h"

class TextureLoader : public AssetLoader
{
    // FIXME: ObjectGen can't detect that a class has unimplemented pure
    // virtuals from the parent.
    CLASS("constructable": false);

public:
    /** Addressing mode for sampling the texture (default Repeat). */
    PROPERTY() GPUAddressMode   addressU;
    PROPERTY() GPUAddressMode   addressV;
    PROPERTY() GPUAddressMode   addressW;

    /** Whether to use an sRGB format (default true). */
    PROPERTY() bool             sRGB;

protected:
                                TextureLoader();
                                ~TextureLoader();

    GPUSamplerDesc              GetSamplerDesc() const;
    PixelFormat                 GetFinalFormat() const;

protected:
    PixelFormat                 mFormat;

};

class Texture2DLoader : public TextureLoader
{
    CLASS();

public:
    AssetPtr                    Load() override;

protected:
                                Texture2DLoader();
                                ~Texture2DLoader();

    /**
     * Load the texture data from the source file. This function is expected
     * to set mWidth, mHeight and mFormat, and populate mTextureData with data
     * for all mip levels that are available (remainder will be generated).
     * Returns whether loaded successfully.
     */
    virtual bool                LoadData() = 0;

protected:
    uint32_t                    mWidth;
    uint32_t                    mHeight;
    std::vector<ByteArray>      mTextureData;

};

class TextureCubeLoader : public TextureLoader
{
    CLASS();

public:
    const char*                 GetExtension() const override;
    AssetPtr                    Load() override;

public:
    PROPERTY() Texture2DPtr     positiveXFace;
    PROPERTY() Texture2DPtr     negativeXFace;
    PROPERTY() Texture2DPtr     positiveYFace;
    PROPERTY() Texture2DPtr     negativeYFace;
    PROPERTY() Texture2DPtr     positiveZFace;
    PROPERTY() Texture2DPtr     negativeZFace;

protected:
                                TextureCubeLoader();
                                ~TextureCubeLoader();

};
