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

#pragma once

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUState.h"

struct GPUPipelineDesc
{
    GPUShader*                      shaders[kGPUShaderStage_NumGraphics];

    /**
     * Argument set layouts for the pipeline. These are shared by all stages.
     * Null indicates that the set is not used by the pipeline.
     */
    GPUArgumentSetLayoutRef         argumentSetLayouts[kMaxArgumentSets];

    GPUBlendStateRef                blendState;
    GPUDepthStencilStateRef         depthStencilState;
    GPURasterizerStateRef           rasterizerState;
    GPURenderTargetStateRef         renderTargetState;
    GPUVertexInputStateRef          vertexInputState;

    GPUPrimitiveTopology            topology;

public:
                                    GPUPipelineDesc();
                                    GPUPipelineDesc(const GPUPipelineDesc& other);

};

DEFINE_HASH_MEM_OPS(GPUPipelineDesc);

inline GPUPipelineDesc::GPUPipelineDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPUPipelineDesc::GPUPipelineDesc(const GPUPipelineDesc& other)
{
    memcpy(this, &other, sizeof(*this));
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
 *   1. Using pre-created GPUPipeline objects (GPUDevice::GetPipeline()). The
 *      pipeline state is created ahead of time, therefore at draw time the
 *      state can just be immediately set without needing any sort of creation.
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
                                    GPUPipeline(GPUDevice&             device,
                                                const GPUPipelineDesc& desc);
                                    ~GPUPipeline();

public:
    const GPUPipelineDesc&          GetDesc() const { return mDesc; }
    GPUPipelineID                   GetID() const   { return mID; }

    /**
     * Return the ID of the shader for a given stage. If the stage is not
     * present, returns numeric_limits<GPUShaderID>::max().
     */
    GPUShaderID                     GetShaderID(const GPUShaderStage stage) const
                                        { return mShaderIDs[stage]; }

    /** Implementation detail for GPUDevice::DropPipeline(). */
    void                            Destroy(OnlyCalledBy<GPUDevice>);

protected:
    const GPUPipelineDesc           mDesc;

    const GPUPipelineID             mID;
    GPUShaderID                     mShaderIDs[kGPUShaderStage_NumGraphics];
};

using GPUPipelineRef = const GPUPipeline*;

struct GPUComputePipelineDesc
{
    GPUShader*                      shader;

    /**
     * Argument set layouts for the pipeline. Null indicates that the set is
     * not used by the pipeline.
     */
    GPUArgumentSetLayoutRef         argumentSetLayouts[kMaxArgumentSets];

public:
                                    GPUComputePipelineDesc();

};

inline GPUComputePipelineDesc::GPUComputePipelineDesc()
{
    memset(this, 0, sizeof(*this));
}

/**
 * GPU compute pipeline state. This is just a combination of a compute shader
 * and argument set layouts. For compute pipelines we only support pre-created
 * pipelines rather than also allowing dynamically creating/caching them as we
 * do for graphics pipelines.
 */
class GPUComputePipeline : public GPUObject
{
public:
                                    ~GPUComputePipeline();

    const GPUComputePipelineDesc&   GetDesc() const { return mDesc; }

protected:
                                    GPUComputePipeline(GPUDevice&                    device,
                                                       const GPUComputePipelineDesc& desc);

protected:
    const GPUComputePipelineDesc    mDesc;

};
