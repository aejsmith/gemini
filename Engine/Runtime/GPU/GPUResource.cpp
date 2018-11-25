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

#include "GPU/GPUResource.h"

GPUResource::GPUResource(GPUDevice&             inDevice,
                         const GPUResourceType  inType,
                         const GPUResourceUsage inUsage) :
    GPUObject (inDevice),
    mType     (inType),
    mUsage    (inUsage)
{
}

#if ORION_BUILD_DEBUG

void GPUResource::ValidateBarrier(const GPUResourceBarrier& inBarrier) const
{
    static const GPUResourceState kMutuallyExclusiveStates[] =
    {
        kGPUResourceState_VertexShaderWrite,
        kGPUResourceState_FragmentShaderWrite,
        kGPUResourceState_ComputeShaderWrite,
        kGPUResourceState_RenderTarget,
        kGPUResourceState_DepthStencilWrite,
        kGPUResourceState_TransferRead,
        kGPUResourceState_TransferWrite,
        kGPUResourceState_Present
    };

    static const GPUResourceState kBufferOnlyStates[] =
    {
        kGPUResourceState_VertexShaderUniformRead,
        kGPUResourceState_FragmentShaderUniformRead,
        kGPUResourceState_ComputeShaderUniformRead,
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

    Assert(inBarrier.newState);

    for (auto state : kMutuallyExclusiveStates)
    {
        if (inBarrier.newState & state && inBarrier.newState & ~state)
        {
            Fatal("GPUResourceBarrier combines mutually exclusive state 0x%x with other states (0x%x)",
                  state,
                  inBarrier.newState);
        }
    }

    if (inBarrier.resource->IsTexture())
    {
        for (auto state : kBufferOnlyStates)
        {
            if (inBarrier.newState & state)
            {
                Fatal("GPUResourceBarrier uses buffer-only state 0x%x on texture (0x%x)",
                      state,
                      inBarrier.newState);
            }
        }
    }
    else
    {
        for (auto state : kTextureOnlyStates)
        {
            if (inBarrier.newState & state)
            {
                Fatal("GPUResourceBarrier uses texture-only state 0x%x on buffer (0x%x)",
                      state,
                      inBarrier.newState);
            }
        }
    }

    // TODO: Could validate against the resource usage flags as well?
}

#endif