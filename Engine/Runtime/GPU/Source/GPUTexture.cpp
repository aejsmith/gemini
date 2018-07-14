/*
 * Copyright (C) 2018 Alex Smith
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

#include "GPU/GPUTexture.h"

GPUTexture::GPUTexture(GPUDevice&            inDevice,
                       const GPUTextureDesc& inDesc) :
    GPUResource     (inDevice,
                     inDesc.type,
                     inDesc.usage),
    mFlags          (inDesc.flags),
    mWidth          (inDesc.width),
    mHeight         (inDesc.height),
    mDepth          (inDesc.depth),
    mArraySize      (inDesc.arraySize),
    mNumMipLevels   (inDesc.numMipLevels),
    mFormat         (inDesc.format)
{

}
