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

#include "GPU/GPUDevice.h"
#include "GPU/GPUTexture.h"

/**
 * Interface for uploading data to and reading data back from GPU resources.
 * Buffers and textures cannot be directly mapped and written to by the CPU,
 * for a number of reasons:
 *
 *  - The majority of resources do not need to be updated by the CPU once
 *    created, so these should live in device-local memory for best performance.
 *    Most/all of this memory is not accessible by the CPU for discrete GPUs.
 *  - For resources which we do want to update from the CPU after creation,
 *    we need to ensure that updates only become visible to the GPU at the
 *    correct point on the GPU timeline. To do direct updates from the CPU we'd
 *    need some sort of versioning for resources (like GL/D3D11 discard maps)
 *    so that we don't write over data currently being consumed by the GPU.
 *    This would involve a lot more usage tracking in the backend, and have
 *    implications for pre-creating descriptor sets etc.
 *
 * So, for now, we just update all persistent resources by writing the data
 * into a staging buffer and using a GPU copy command to transfer to the real
 * resource. Note that vertex and index buffers do have a transient path
 * (GPUGraphicsCommandList::Write*Buffer()), which does not require a GPUBuffer
 * to be created.
 *
 * Usage for uploading data is:
 *  - Call Initialise(). This specifies the resource properties and allocates
 *    the staging buffer. This can be called again after uploading to reuse
 *    the same object for multiple uploads.
 *  - Use MapWrite()/Write() to supply the data to upload into the resource.
 *  - Call Finalise(). Once this is called, the data cannot be modified. It
 *    must be called before the resource is passed to a command for uploading.
 *    This is really only for validation purposes to ensure correct usage so
 *    that we don't modify any data in use by the GPU.
 *  - Pass the staging resource to a command to perform the upload.
 *
 * Usage for reading back data is:
 *  - Call Initialise().
 *  - Pass the staging resource to a command to perform the read back.
 *  - Wait for the submitted command to complete (TODO: How? Set a flag upon
 *    completion of the commands in the backend, i.e. when the fence has been
 *    signalled?).
 *  - Use MapRead() to obtain the data.
 */
class GPUStagingResource : public GPUDeviceChild
{
protected:
                            GPUStagingResource();
                            ~GPUStagingResource();

public:
    GPUStagingAccess        GetAccess() const   { return mAccess; }
    bool                    IsAllocated() const { return mHandle != nullptr; }
    bool                    IsFinalised() const { return mFinalised; }

    /** Get the staging buffer handle allocated by GPUStagingPool. */
    void*                   GetHandle() const   { return mHandle; }

    void                    Finalise();

protected:
    void                    Allocate(const GPUStagingAccess inAccess,
                                     const uint32_t         inSize);

protected:
    GPUStagingAccess        mAccess;
    void*                   mHandle;
    void*                   mMapping;
    bool                    mFinalised;

};

class GPUStagingBuffer : public GPUStagingResource
{
public:
                            GPUStagingBuffer();

                            GPUStagingBuffer(const GPUStagingAccess inAccess,
                                             const uint32_t         inSize);

                            ~GPUStagingBuffer() {}

public:
    uint32_t                GetSize() const     { return mSize; }

    /**
     * Allocate a new staging buffer. Any previous buffer is discarded (once
     * any previously submitted GPU transfers have completed), and the buffer
     * becomes free to use again.
     */
    void                    Initialise(const GPUStagingAccess inAccess,
                                       const uint32_t         inSize);

    /**
     * Return a pointer to write data into. Buffer must not be finalised, and
     * must have been initialised with kGPUStagingAccess_Write.
     */
    void*                   MapWrite();

    /**
     * Copy data from elsewhere into the buffer. Same rules apply as for
     * MapWrite().
     */
    void                    Write(const void*    inData,
                                  const uint32_t inSize,
                                  const uint32_t inOffset = 0);

private:
    uint32_t                mSize;

};

class GPUStagingTexture : public GPUStagingResource
{
public:
                            GPUStagingTexture();

                            GPUStagingTexture(const GPUStagingAccess inAccess,
                                              const GPUTextureDesc&  inDesc);

                            ~GPUStagingTexture();

public:
    PixelFormat             GetFormat() const       { return mDesc.format; }
    uint32_t                GetWidth() const        { return mDesc.width; }
    uint32_t                GetHeight() const       { return mDesc.height; }
    uint32_t                GetDepth() const        { return mDesc.depth; }
    uint16_t                GetArraySize() const    { return mDesc.arraySize; }
    uint8_t                 GetNumMipLevels() const { return mDesc.numMipLevels; }

    uint32_t                GetMipWidth(const uint8_t inMip) const;
    uint32_t                GetMipHeight(const uint8_t inMip) const;
    uint32_t                GetMipDepth(const uint8_t inMip) const;

    /**
     * Allocate a new staging texture. Any previous texture is discarded (once
     * any previously submitted GPU transfers have completed), and the texture
     * becomes free to use again.
     *
     * The supplied GPUTextureDesc specifies properties of the staging texture.
     * The type, usage and flags members are ignored, only the format,
     * dimensions and number of subresources are relevant.
     */
    void                    Initialise(const GPUStagingAccess inAccess,
                                       const GPUTextureDesc&  inDesc);

    /**
     * Return a pointer to write data into for a subresource. Texture must not
     * be finalised, and must have been initialised with kGPUStagingAccess_Write.
     * Data layout is linear: consecutive pixels of a row are contiguous in
     * memory, and each row is contiguous. Number of bytes per pixel is as
     * reported by PixelFormatInfo::BytesPerPixel().
     */
    void*                   MapWrite(const GPUSubresource inSubresource);

    /**
     * Calculate the offset in the underlying staging buffer of a given
     * subresource.
     */
    uint32_t                GetSubresourceOffset(const GPUSubresource inSubresource) const;

private:
    uint32_t                GetSubresourceIndex(const GPUSubresource inSubresource) const;

private:
    GPUTextureDesc          mDesc;
    std::vector<uint32_t>   mSubresourceOffsets;

};

/**
 * Interface for allocating memory for staging resources. The GPUStaging*
 * classes are all backend-agnostic, and rely on this class to do the API-
 * specific memory allocation.
 */
class GPUStagingPool : public GPUDeviceChild
{
protected:
                            GPUStagingPool(GPUDevice& inDevice);
                            ~GPUStagingPool() {}

public:
    /**
     * Allocates memory for a staging resource. Returns a handle referring to
     * the allocation, which gets stored in the GPUStagingResource.
     */
    virtual void*           Allocate(const GPUStagingAccess inAccess,
                                     const uint32_t         inSize,
                                     void*&                 outMapping) = 0;

    /**
     * Free a staging resource allocation. Will only free the allocation once
     * the memory is guaranteed to no longer be in use by the GPU.
     */
    virtual void            Free(void* const inHandle) = 0;

};

inline GPUStagingResource::GPUStagingResource() :
    GPUDeviceChild  (GPUDevice::Get()),
    mHandle         (nullptr),
    mMapping        (nullptr),
    mFinalised      (false)
{
}

inline void GPUStagingResource::Finalise()
{
    Assert(IsAllocated());
    Assert(!IsFinalised());

    mFinalised = true;
}

inline GPUStagingBuffer::GPUStagingBuffer() :
    GPUStagingResource  (),
    mSize               (0)
{
}

inline GPUStagingBuffer::GPUStagingBuffer(const GPUStagingAccess inAccess,
                                          const uint32_t         inSize) :
    GPUStagingBuffer    ()
{
    Initialise(inAccess, inSize);
}

inline void GPUStagingBuffer::Initialise(const GPUStagingAccess inAccess,
                                         const uint32_t         inSize)
{
    Allocate(inAccess, inSize);

    mSize = inSize;
}

inline void* GPUStagingBuffer::MapWrite()
{
    Assert(IsAllocated());
    Assert(!IsFinalised());

    return mMapping;
}

inline void GPUStagingBuffer::Write(const void*    inData,
                                    const uint32_t inSize,
                                    const uint32_t inOffset)
{
    Assert(IsAllocated());
    Assert(!IsFinalised());
    Assert(inOffset + inSize <= mSize);

    memcpy(reinterpret_cast<uint8_t*>(mMapping) + inOffset, inData, inSize);
}

inline GPUStagingTexture::GPUStagingTexture(const GPUStagingAccess inAccess,
                                            const GPUTextureDesc&  inDesc) :
    GPUStagingTexture   ()
{
    Initialise(inAccess, inDesc);
}

inline uint32_t GPUStagingTexture::GetMipWidth(const uint8_t inMip) const
{
    return std::max(mDesc.width >> inMip, 1u);
}

inline uint32_t GPUStagingTexture::GetMipHeight(const uint8_t inMip) const
{
    return std::max(mDesc.height >> inMip, 1u);
}

inline uint32_t GPUStagingTexture::GetMipDepth(const uint8_t inMip) const
{
    return std::max(mDesc.depth >> inMip, 1u);
}

inline uint32_t GPUStagingTexture::GetSubresourceOffset(const GPUSubresource inSubresource) const
{
    return mSubresourceOffsets[GetSubresourceIndex(inSubresource)];
}

inline uint32_t GPUStagingTexture::GetSubresourceIndex(const GPUSubresource inSubresource) const
{
    Assert(IsAllocated());
    Assert(inSubresource.mipLevel < GetNumMipLevels());
    Assert(inSubresource.layer < GetArraySize());

    return (inSubresource.layer * GetNumMipLevels()) + inSubresource.mipLevel;
}

inline GPUStagingPool::GPUStagingPool(GPUDevice& inDevice) :
    GPUDeviceChild  (inDevice)
{
}
