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

#pragma once

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUState.h"

struct GPUPipelineDesc
{
    GPUShader*                  shaders[kGPUShaderStage_NumGraphics];

    /**
     * Argument set layouts for the pipeline. These are shared by all stages.
     * Null indicates that the set is not used by the pipeline.
     */
    GPUArgumentSetLayout*       argumentSetLayouts[kMaxArgumentSets];

    GPUBlendStateRef            blendState;
    GPUDepthStencilStateRef     depthStencilState;
    GPURasterizerStateRef       rasterizerState;
    GPURenderTargetStateRef     renderTargetState;
    GPUVertexInputStateRef      vertexInputState;

    GPUPrimitiveTopology        topology;

public:
                                GPUPipelineDesc();
                                GPUPipelineDesc(const GPUPipelineDesc& inOther);

};

DEFINE_HASH_MEM_OPS(GPUPipelineDesc);

inline GPUPipelineDesc::GPUPipelineDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPUPipelineDesc::GPUPipelineDesc(const GPUPipelineDesc& inOther)
{
    memcpy(this, &inOther, sizeof(*this));
}

/**
 * GPU graphics pipeline state. This encapsulates the majority of the state for
 * the graphics pipeline needed for a draw call.
 *
 * Creation of pipeline states is an expensive operation: it likely includes
 * the compilation of shaders into GPU-specific code by the driver.
 *
 * There are two mechanisms for setting pipeline state on a command list.
 *
 *   1. Using pre-created GPUPipeline objects. The pipeline state is created
 *      ahead of time, therefore at draw time the state can just be immediately
 *      set without needing any sort of creation.
 *   2. Dynamically through the pipeline cache. The command list is supplied
 *      with a GPUPipelineDesc describing the pipeline state, and internally a
 *      matching pipeline will be looked up in the cache. If no matching
 *      pipeline is found, then a new one will be created.
 *
 * Pre-created pipelines should be preferred, since they won't result in draw-
 * time hitching if a new pipeline needs to be created.
 */
class GPUPipeline : public GPUObject
{
protected:
                                GPUPipeline(GPUDevice&             inDevice,
                                            const GPUPipelineDesc& inDesc);
                                ~GPUPipeline();

public:
    const GPUPipelineDesc&      GetDesc() const { return mDesc; }

    /** Implementation detail for GPUDevice::CreatePipeline(). */
    ReferencePtr<GPUPipeline>   CreatePtr(OnlyCalledBy<GPUDevice>);

    /** Implementation detail for GPUDevice::DropPipeline(). */
    void                        Destroy(OnlyCalledBy<GPUDevice>);

protected:
    void                        Released() override;

protected:
    const GPUPipelineDesc       mDesc;

};

using GPUPipelinePtr = ReferencePtr<GPUPipeline>;
