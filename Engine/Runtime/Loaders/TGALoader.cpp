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

#include "Loaders/TGALoader.h"

/*
 * TODO:
 *  - Support compressed images and 16bpp images (need 16-bit packed pixel
 *    formats).
 */

bool TGALoader::LoadData()
{
    Header header;
    if (!mData->Read(reinterpret_cast<char*>(&header), sizeof(header), 0))
    {
        LogError("%s: Failed to read asset data", mPath);
        return false;
    }

    /* Only support uncompressed RGB images for now. */
    if (header.imageType != 2)
    {
        LogError("%s: Unsupported image format (%u)", mPath, header.imageType);
        return false;
    }

    if (header.depth != 24 && header.depth != 32)
    {
        LogError("%s: Unsupported depth (%u)", mPath, header.depth);
        return false;
    }

    /* Determine image properties. */
    mWidth  = header.width;
    mHeight = header.height;
    mFormat = kPixelFormat_B8G8R8A8;

    /* Read in the top mip data, which is after the ID and colour map. */
    const uint32_t fileSize   = mWidth *
                                mHeight *
                                (header.depth / 8);
    const uint32_t fileOffset = sizeof(header) +
                                header.idLength +
                                (header.colourMapLength * (header.colourMapDepth / 8));

    ByteArray fileData(fileSize);

    if (!mData->Read(fileData.Get(), fileSize, fileOffset))
    {
        LogError("%s: Failed to read asset data", mPath);
        return false;
    }

    if (header.depth == 24)
    {
        /* We don't have 24-bit colour formats, so add an alpha channel. */
        ByteArray convertedData(mWidth * mHeight * 4);

        for (uint32_t i = 0; i < mWidth * mHeight; i++)
        {
            convertedData[(i * 4) + 0] = fileData[(i * 3) + 0];
            convertedData[(i * 4) + 1] = fileData[(i * 3) + 1];
            convertedData[(i * 4) + 2] = fileData[(i * 3) + 2];
            convertedData[(i * 4) + 3] = 255;
        }

        mTextureData.emplace_back(std::move(convertedData));
    }
    else
    {
        mTextureData.emplace_back(std::move(fileData));
    }

    return true;
}
