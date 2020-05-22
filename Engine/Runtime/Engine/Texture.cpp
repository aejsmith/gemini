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

#include "Engine/Texture.h"

#include "Core/Thread.h"

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUResourceView.h"
#include "GPU/GPUSampler.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUTexture.h"

TextureBase::TextureBase() :
    mTexture        (nullptr),
    mResourceView   (nullptr),
    mSampler        (nullptr)
{
}

void TextureBase::CreateTexture(const GPUTextureDesc&     textureDesc,
                                const GPUSamplerDesc&     samplerDesc,
                                const GPUResourceViewType viewType)
{
    // TODO: Async texture creation/upload/mipgen.
    Assert(Thread::IsMain());

    mTexture = GPUDevice::Get().CreateTexture(textureDesc);
    mSampler = GPUDevice::Get().GetSampler(samplerDesc);

    mNumMipLevels = mTexture->GetNumMipLevels();
    mFormat       = mTexture->GetFormat();

    GPUResourceViewDesc viewDesc;
    viewDesc.type          = viewType;
    viewDesc.usage         = kGPUResourceUsage_ShaderRead;
    viewDesc.format        = mFormat;
    viewDesc.mipOffset     = 0;
    viewDesc.mipCount      = mNumMipLevels;
    viewDesc.elementOffset = 0;
    viewDesc.elementCount  = mTexture->GetArraySize();

    mResourceView = GPUDevice::Get().CreateResourceView(mTexture, viewDesc);
}

TextureBase::~TextureBase()
{
    delete mResourceView;
    delete mTexture;
}

void TextureBase::PathChanged()
{
    if (GEMINI_GPU_MARKERS && mTexture && IsManaged())
    {
        mTexture->SetName(GetPath());
    }
}

Texture2D::Texture2D(const uint32_t                width,
                     const uint32_t                height,
                     const uint8_t                 numMipLevels,
                     const PixelFormat             format,
                     const GPUSamplerDesc&         samplerDesc,
                     const std::vector<ByteArray>& data) :
    mWidth  (width),
    mHeight (height)
{
    GPUTextureDesc textureDesc;
    textureDesc.type         = kGPUResourceType_Texture2D;
    textureDesc.usage        = kGPUResourceUsage_ShaderRead;
    textureDesc.flags        = kGPUTexture_None;
    textureDesc.format       = format;
    textureDesc.width        = width;
    textureDesc.height       = height;
    textureDesc.depth        = 1;
    textureDesc.arraySize    = 1;
    textureDesc.numMipLevels = numMipLevels;

    CreateTexture(textureDesc, samplerDesc, kGPUResourceViewType_Texture2D);

    Assert(data.size() >= 1);
    Assert(data.size() <= mNumMipLevels);

    /* Upload what we have data for. */
    GPUGraphicsContext& context = GPUGraphicsContext::Get();

    context.ResourceBarrier(mTexture,
                            kGPUResourceState_None,
                            kGPUResourceState_TransferWrite);

    for (uint8_t mipLevel = 0; mipLevel < data.size(); mipLevel++)
    {
        textureDesc.width        = mTexture->GetMipWidth(mipLevel);
        textureDesc.height       = mTexture->GetMipHeight(mipLevel);
        textureDesc.numMipLevels = 1;

        GPUStagingTexture stagingTexture(kGPUStagingAccess_Write, textureDesc);

        const uint32_t mipDataSize = textureDesc.width *
                                     textureDesc.height *
                                     PixelFormatInfo::BytesPerPixel(GetFormat());

        Assert(data[mipLevel].GetSize() == mipDataSize);

        memcpy(stagingTexture.MapWrite(GPUSubresource{0, 0}),
               data[mipLevel].Get(),
               mipDataSize);

        stagingTexture.Finalise();

        context.UploadTexture(mTexture,
                              GPUSubresource{mipLevel, 0},
                              glm::ivec3(0, 0, 0),
                              stagingTexture,
                              GPUSubresource{0, 0},
                              glm::ivec3(0, 0, 0),
                              glm::ivec3(textureDesc.width, textureDesc.height, 1));
    }

    /* Generate mips for the rest. */
    for (uint8_t mipLevel = data.size(); mipLevel < mNumMipLevels; mipLevel++)
    {
        const uint8_t sourceMipLevel = mipLevel - 1;

        GPUResourceBarrier sourceBarrier{};
        sourceBarrier.resource     = mTexture;
        sourceBarrier.range        = { sourceMipLevel, 1, 0, 1 };
        sourceBarrier.currentState = kGPUResourceState_TransferWrite;
        sourceBarrier.newState     = kGPUResourceState_TransferRead;

        context.ResourceBarrier(&sourceBarrier, 1);

        /* Scaled blit from previous mip level. */
        context.BlitTexture(mTexture,
                            GPUSubresource{mipLevel, 0},
                            mTexture,
                            GPUSubresource{sourceMipLevel, 0});

        /* Swap back to TransferWrite so that everything can be transitioned in
         * one go at the end. */
        std::swap(sourceBarrier.currentState, sourceBarrier.newState);

        context.ResourceBarrier(&sourceBarrier, 1);
    }

    /* After creation, we always keep the texture in AllShaderRead. */
    context.ResourceBarrier(mTexture,
                            kGPUResourceState_TransferWrite,
                            kGPUResourceState_AllShaderRead);
}

Texture2D::~Texture2D()
{
}

TextureCube::TextureCube(const Texture2DPtr    (&textures)[kGPUCubeFaceCount],
                         const GPUSamplerDesc& samplerDesc)
{
    GPUTextureDesc textureDesc;
    textureDesc.type      = kGPUResourceType_Texture2D;
    textureDesc.usage     = kGPUResourceUsage_ShaderRead;
    textureDesc.flags     = kGPUTexture_CubeCompatible;
    textureDesc.depth     = 1;
    textureDesc.arraySize = 6;

    for (unsigned face = 0; face < kGPUCubeFaceCount; face++)
    {
        Assert(textures[face]);
        Assert(textures[face]->GetWidth() == textures[face]->GetHeight());

        if (face == 0)
        {
            mSize = textures[face]->GetWidth();

            textureDesc.width        = mSize;
            textureDesc.height       = mSize;
            textureDesc.numMipLevels = textures[face]->GetNumMipLevels();
            textureDesc.format       = textures[face]->GetFormat();
        }
        else
        {
            Assert(textures[face]->GetWidth() == mSize);
            Assert(textures[face]->GetNumMipLevels() == textureDesc.numMipLevels);
            Assert(textures[face]->GetFormat() == textureDesc.format);
        }
    }

    CreateTexture(textureDesc, samplerDesc, kGPUResourceViewType_TextureCube);

    GPUGraphicsContext& context = GPUGraphicsContext::Get();

    context.ResourceBarrier(mTexture,
                            kGPUResourceState_None,
                            kGPUResourceState_TransferWrite);

    for (unsigned face = 0; face < kGPUCubeFaceCount; face++)
    {
        GPUTexture* const sourceTexture = textures[face]->GetTexture();

        context.ResourceBarrier(sourceTexture,
                                kGPUResourceState_AllShaderRead,
                                kGPUResourceState_TransferRead);

        for (unsigned mipLevel = 0; mipLevel < mNumMipLevels; mipLevel++)
        {
            context.BlitTexture(mTexture,
                                { mipLevel, face },
                                sourceTexture,
                                { mipLevel, 0 });
        }

        context.ResourceBarrier(sourceTexture,
                                kGPUResourceState_TransferRead,
                                kGPUResourceState_AllShaderRead);
    }

    context.ResourceBarrier(mTexture,
                            kGPUResourceState_TransferWrite,
                            kGPUResourceState_AllShaderRead);
}

TextureCube::~TextureCube()
{
}
