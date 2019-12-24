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
#include "Core/PixelFormat.h"

#include "Engine/Asset.h"

#include "GPU/GPUSampler.h"

class GPUResourceView;
class GPUTexture;

struct GPUTextureDesc;

/**
 * Base texture asset class. Texture assets are read-only, generally used for
 * texture data loaded from disk. Materials can reference them. Custom
 * rendering code that needs to dynamically update textures or write them from
 * the GPU should use GPUTexture/RenderGraph instead.
 *
 * Once created, the underlying GPUTexture for a texture asset is always in the
 * kGPUResourceState_AllShaderRead state.
 */
class TextureBase : public Asset
{
    CLASS();

public:
    GPUTexture*                 GetTexture() const      { return mTexture; }
    GPUResourceView*            GetResourceView() const { return mResourceView; }
    GPUSamplerRef               GetSampler() const      { return mSampler; }

    uint8_t                     GetNumMipLevels() const { return mNumMipLevels; }
    PixelFormat                 GetFormat() const       { return mFormat; }

protected:
                                TextureBase();
                                ~TextureBase();

    void                        CreateTexture(const GPUTextureDesc&     inTextureDesc,
                                              const GPUSamplerDesc&     inSamplerDesc,
                                              const GPUResourceViewType inViewType);

protected:
    GPUTexture*                 mTexture;
    GPUResourceView*            mResourceView;
    GPUSamplerRef               mSampler;

    uint8_t                     mNumMipLevels;
    PixelFormat                 mFormat;

};

using TextureBasePtr = ObjPtr<TextureBase>;

/** 2D texture asset class. */
class Texture2D final : public TextureBase
{
    CLASS();

public:
    /**
     * Constructs a new texture with the given parameters. inNumMipLevels == 0
     * will create a full mip chain. inData supplies the data for each mip
     * level. At least one mip level's data must be supplied. If the number of
     * mip levels in inData is less than the number of mip levels the texture
     * has, then remaining levels will be generated.
     */
                                Texture2D(const uint32_t                inWidth,
                                          const uint32_t                inHeight,
                                          const uint8_t                 inNumMipLevels,
                                          const PixelFormat             inFormat,
                                          const GPUSamplerDesc&         inSamplerDesc,
                                          const std::vector<ByteArray>& inData);

    uint32_t                    GetWidth() const    { return mWidth; }
    uint32_t                    GetHeight() const   { return mHeight; }

protected:
                                ~Texture2D();

private:
    uint32_t                    mWidth;
    uint32_t                    mHeight;

};

using Texture2DPtr = ObjPtr<Texture2D>;
