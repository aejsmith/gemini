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

void TextureBase::CreateTexture(const GPUTextureDesc&     inTextureDesc,
                                const GPUSamplerDesc&     inSamplerDesc,
                                const GPUResourceViewType inViewType)
{
    // TODO: Async texture creation/upload/mipgen.
    Assert(Thread::IsMain());

    mTexture = GPUDevice::Get().CreateTexture(inTextureDesc);
    mSampler = GPUDevice::Get().GetSampler(inSamplerDesc);

    mNumMipLevels = mTexture->GetNumMipLevels();
    mFormat       = mTexture->GetFormat();

    GPUResourceViewDesc viewDesc;
    viewDesc.type          = inViewType;
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

Texture2D::Texture2D(const uint32_t                inWidth,
                     const uint32_t                inHeight,
                     const uint8_t                 inNumMipLevels,
                     const PixelFormat             inFormat,
                     const GPUSamplerDesc&         inSamplerDesc,
                     const std::vector<ByteArray>& inData) :
    mWidth  (inWidth),
    mHeight (inHeight)
{
    GPUTextureDesc textureDesc;
    textureDesc.type         = kGPUResourceType_Texture2D;
    textureDesc.usage        = kGPUResourceUsage_ShaderRead;
    textureDesc.flags        = kGPUTexture_None;
    textureDesc.format       = inFormat;
    textureDesc.width        = inWidth;
    textureDesc.height       = inHeight;
    textureDesc.depth        = 1;
    textureDesc.arraySize    = 1;
    textureDesc.numMipLevels = inNumMipLevels;

    CreateTexture(textureDesc, inSamplerDesc, kGPUResourceViewType_Texture2D);

    Assert(inData.size() >= 1);
    Assert(inData.size() < mNumMipLevels);

    /* Upload what we have data for. */
    GPUGraphicsContext& graphicsContext = GPUGraphicsContext::Get();

    graphicsContext.ResourceBarrier(mTexture,
                                    kGPUResourceState_None,
                                    kGPUResourceState_TransferWrite);

    for (uint8_t mipLevel = 0; mipLevel < inData.size(); mipLevel++)
    {
        textureDesc.width        = mTexture->GetMipWidth(mipLevel);
        textureDesc.height       = mTexture->GetMipHeight(mipLevel);
        textureDesc.numMipLevels = 1;

        GPUStagingTexture stagingTexture(kGPUStagingAccess_Write, textureDesc);

        const uint32_t mipDataSize = textureDesc.width *
                                     textureDesc.height *
                                     PixelFormatInfo::BytesPerPixel(GetFormat());

        Assert(inData[mipLevel].GetSize() == mipDataSize);

        memcpy(stagingTexture.MapWrite(GPUSubresource{0, 0}),
               inData[mipLevel].Get(),
               mipDataSize);

        stagingTexture.Finalise();

        graphicsContext.UploadTexture(mTexture,
                                      GPUSubresource{mipLevel, 0},
                                      glm::ivec3(0, 0, 0),
                                      stagingTexture,
                                      GPUSubresource{0, 0},
                                      glm::ivec3(0, 0, 0),
                                      glm::ivec3(textureDesc.width, textureDesc.height, 1));
    }

    /* Generate mips for the rest. */
    for (uint8_t mipLevel = inData.size(); mipLevel < mNumMipLevels; mipLevel++)
    {
        const uint8_t sourceMipLevel = mipLevel - 1;

        GPUResourceBarrier sourceBarrier{};
        sourceBarrier.resource     = mTexture;
        sourceBarrier.range        = { sourceMipLevel, 1, 0, 1 };
        sourceBarrier.currentState = kGPUResourceState_TransferWrite;
        sourceBarrier.newState     = kGPUResourceState_TransferRead;

        graphicsContext.ResourceBarrier(&sourceBarrier, 1);

        /* Scaled blit from previous mip level. */
        graphicsContext.BlitTexture(mTexture,
                                    GPUSubresource{mipLevel, 0},
                                    mTexture,
                                    GPUSubresource{sourceMipLevel, 0});

        /* Swap back to TransferWrite so that everything can be transitioned in
         * one go at the end. */
        std::swap(sourceBarrier.currentState, sourceBarrier.newState);

        graphicsContext.ResourceBarrier(&sourceBarrier, 1);
    }

    /* After creation, we always keep the texture in AllShaderRead. */
    graphicsContext.ResourceBarrier(mTexture,
                                    kGPUResourceState_TransferWrite,
                                    kGPUResourceState_AllShaderRead);
}

Texture2D::~Texture2D()
{
}
