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

#include "GPU/GPUUtils.h"

size_t GPUUtils::GetAttributeSize(const GPUAttributeFormat format)
{
    switch (format)
    {
        case kGPUAttributeFormat_R8_UNorm:              return 1;
        case kGPUAttributeFormat_R8G8_UNorm:            return 2;
        case kGPUAttributeFormat_R8G8B8_UNorm:          return 3;
        case kGPUAttributeFormat_R8G8B8A8_UNorm:        return 4;

        case kGPUAttributeFormat_R8_UInt:               return 1;
        case kGPUAttributeFormat_R8G8_UInt:             return 2;
        case kGPUAttributeFormat_R8G8B8_UInt:           return 3;
        case kGPUAttributeFormat_R8G8B8A8_UInt:         return 4;

        case kGPUAttributeFormat_R16_UNorm:             return 2;
        case kGPUAttributeFormat_R16G16_UNorm:          return 4;
        case kGPUAttributeFormat_R16G16B16_UNorm:       return 6;
        case kGPUAttributeFormat_R16G16B16A16_UNorm:    return 8;

        case kGPUAttributeFormat_R16_UInt:              return 2;
        case kGPUAttributeFormat_R16G16_UInt:           return 4;
        case kGPUAttributeFormat_R16G16B16_UInt:        return 6;
        case kGPUAttributeFormat_R16G16B16A16_UInt:     return 8;

        case kGPUAttributeFormat_R32_UInt:              return 4;
        case kGPUAttributeFormat_R32G32_UInt:           return 8;
        case kGPUAttributeFormat_R32G32B32_UInt:        return 12;
        case kGPUAttributeFormat_R32G32B32A32_UInt:     return 16;

        case kGPUAttributeFormat_R32_Float:             return 4;
        case kGPUAttributeFormat_R32G32_Float:          return 8;
        case kGPUAttributeFormat_R32G32B32_Float:       return 12;
        case kGPUAttributeFormat_R32G32B32A32_Float:    return 16;

        default:
            UnreachableMsg("Unknown attribute format");

    }
}

#if GEMINI_BUILD_DEBUG

void GPUUtils::ValidateResourceState(const GPUResourceState state,
                                     const bool             isTexture)
{
    static const GPUResourceState kMutuallyExclusiveStates[] =
    {
        kGPUResourceState_VertexShaderWrite,
        kGPUResourceState_PixelShaderWrite,
        kGPUResourceState_ComputeShaderWrite,
        kGPUResourceState_RenderTarget,
        kGPUResourceState_DepthStencilWrite,
        kGPUResourceState_TransferRead,
        kGPUResourceState_TransferWrite,
        kGPUResourceState_Present
    };

    static const GPUResourceState kBufferOnlyStates[] =
    {
        kGPUResourceState_VertexShaderConstantRead,
        kGPUResourceState_PixelShaderConstantRead,
        kGPUResourceState_ComputeShaderConstantRead,
        kGPUResourceState_IndirectBufferRead,
        kGPUResourceState_VertexBufferRead,
        kGPUResourceState_IndexBufferRead
    };

    static const GPUResourceState kTextureOnlyStates[] =
    {
        kGPUResourceState_RenderTarget,
        kGPUResourceState_DepthStencilWrite,
        kGPUResourceState_DepthReadStencilWrite,
        kGPUResourceState_DepthWriteStencilRead,
        kGPUResourceState_DepthStencilRead
    };

    Assert(state != 0);

    for (auto otherState : kMutuallyExclusiveStates)
    {
        if (state & otherState && state & ~otherState)
        {
            Fatal("GPUResourceState combines mutually exclusive state 0x%x with other states (0x%x)",
                  otherState,
                  state);
        }
    }

    if (isTexture)
    {
        for (auto otherState : kBufferOnlyStates)
        {
            if (state & otherState)
            {
                Fatal("GPUResourceState uses buffer-only state 0x%x on texture (0x%x)",
                      otherState,
                      state);
            }
        }
    }
    else
    {
        for (auto otherState : kTextureOnlyStates)
        {
            if (state & otherState)
            {
                Fatal("GPUResourceState uses texture-only state 0x%x on buffer (0x%x)",
                      otherState,
                      state);
            }
        }
    }
}

#endif
