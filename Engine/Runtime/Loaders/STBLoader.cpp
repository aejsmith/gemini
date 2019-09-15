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

#include "Loaders/STBLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) Assert(x);
#define STBI_NO_STDIO
#include "stb_image.h"

static int STBImageRead(void* const inUser,
                        char*       outData,
                        const int   inSize)
{
    STBLoader* const loader = reinterpret_cast<STBLoader*>(inUser);
    DataStream* const data  = loader->GetData();

    const uint64_t size          = data->GetSize();
    const uint64_t offset        = std::min(size, data->GetOffset());
    const uint64_t remainingSize = size - offset;
    const uint64_t readSize      = std::min(remainingSize, static_cast<uint64_t>(inSize));

    if (data->Read(outData, readSize))
    {
        return readSize;
    }
    else
    {
        return 0;
    }
}

static void STBImageSkip(void* const inUser,
                         const int   inNumBytes)
{
    STBLoader* const loader = reinterpret_cast<STBLoader*>(inUser);
    DataStream* const data  = loader->GetData();

    data->Seek(kSeekMode_Current, static_cast<int64_t>(inNumBytes));
}

static int STBImageEOF(void* const inUser)
{
    STBLoader* const loader = reinterpret_cast<STBLoader*>(inUser);
    DataStream* const data  = loader->GetData();

    return (data->GetOffset() >= data->GetSize()) ? 1 : 0;
}

static const stbi_io_callbacks sSTBImageIOCallbacks =
{
    STBImageRead,
    STBImageSkip,
    STBImageEOF
};

bool STBLoader::LoadData()
{
    int width, height, origChannels;

    /* Force conversion to 4 channels as we don't have 3 channel pixel formats.
     * Alpha channel will be filled with 1. TODO: What about 2 channel images,
     * do we want to support that? */
    stbi_uc* const image = stbi_load_from_callbacks(&sSTBImageIOCallbacks,
                                                    this,
                                                    &width,
                                                    &height,
                                                    &origChannels,
                                                    4);

    if (!image)
    {
        /* This isn't threadsafe but eh, don't want to slap a lock around
         * loading just for an error case. */
        LogError("%s: Failed to load image data: %s", mPath, stbi_failure_reason());
        return false;
    }

    auto guard = MakeScopeGuard([image] { stbi_image_free(image); });

    mWidth  = width;
    mHeight = height;
    mFormat = kPixelFormat_R8G8B8A8;

    const uint32_t dataSize = width * height * 4;

    ByteArray fileData(dataSize);
    memcpy(fileData.Get(), image, dataSize);
    mTextureData.emplace_back(std::move(fileData));

    return true;
}
