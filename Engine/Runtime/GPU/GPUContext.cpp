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

#include "GPU/GPUContext.h"

#include "GPU/GPUTexture.h"

void GPUTransferContext::BlitTexture(GPUTexture* const    inDestTexture,
                                     const GPUSubresource inDestSubresource,
                                     GPUTexture* const    inSourceTexture,
                                     const GPUSubresource inSourceSubresource)
{
    const glm::ivec3 destSize(
        inDestTexture->GetMipWidth (inDestSubresource.mipLevel),
        inDestTexture->GetMipHeight(inDestSubresource.mipLevel),
        inDestTexture->GetMipDepth (inDestSubresource.mipLevel)
    );

    const glm::ivec3 sourceSize(
        inSourceTexture->GetMipWidth (inSourceSubresource.mipLevel),
        inSourceTexture->GetMipHeight(inSourceSubresource.mipLevel),
        inSourceTexture->GetMipDepth (inSourceSubresource.mipLevel)
    );

    BlitTexture(inDestTexture,
                inDestSubresource,
                glm::ivec3(0, 0, 0),
                destSize,
                inSourceTexture,
                inSourceSubresource,
                glm::ivec3(0, 0, 0),
                sourceSize);
}
