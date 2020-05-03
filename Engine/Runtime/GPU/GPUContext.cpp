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

#include "GPU/GPUContext.h"

#include "GPU/GPUTexture.h"

void GPUTransferContext::BlitTexture(GPUTexture* const    destTexture,
                                     const GPUSubresource destSubresource,
                                     GPUTexture* const    sourceTexture,
                                     const GPUSubresource sourceSubresource)
{
    const glm::ivec3 destSize(
        destTexture->GetMipWidth (destSubresource.mipLevel),
        destTexture->GetMipHeight(destSubresource.mipLevel),
        destTexture->GetMipDepth (destSubresource.mipLevel)
    );

    const glm::ivec3 sourceSize(
        sourceTexture->GetMipWidth (sourceSubresource.mipLevel),
        sourceTexture->GetMipHeight(sourceSubresource.mipLevel),
        sourceTexture->GetMipDepth (sourceSubresource.mipLevel)
    );

    BlitTexture(destTexture,
                destSubresource,
                glm::ivec3(0, 0, 0),
                destSize,
                sourceTexture,
                sourceSubresource,
                glm::ivec3(0, 0, 0),
                sourceSize);
}
