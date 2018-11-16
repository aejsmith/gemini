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
#include "GPU/GPUBuffer.h"
#include "GPU/GPUPipeline.h"
#include "GPU/GPUResourceView.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUTexture.h"

#include "Core/HashTable.h"
#include "Core/Singleton.h"

#include <shared_mutex>

class GPUGraphicsCommandList;
class GPUGraphicsContext;
class GPUUniformPool;
class Window;

/**
 * This class is the main class of the low-level rendering API abstraction
 * layer. It provides an interface to create GPU resources and record
 * commands.
 */
class GPUDevice : public Singleton<GPUDevice>
{
protected:
                                    GPUDevice();
                                    ~GPUDevice();

public:
    static void                     Create();

    GPUVendor                       GetVendor() const           { return mVendor; }

    /** Get the primary graphics context. This is always present. */
    GPUGraphicsContext&             GetGraphicsContext() const  { return *mGraphicsContext; }

    /** Get the uniform data pool. */
    GPUUniformPool&                 GetUniformPool() const      { return *mUniformPool; }

    /**
     * Mark the end of a frame. Should be called after the last work submission
     * in the frame (usually an EndPresent()). Any recorded work that has not
     * yet been submitted to the device on each context will be submitted.
     *
     * This function can block waiting for GPU work from earlier frames to
     * complete, so that the CPU does not get too far ahead of the GPU.
     *
     * No other threads must be recording work to any command lists at the
     * point where this is called.
     */
    void                            EndFrame();

    /** Helper to look up a cached pipeline for GPUGraphicsCommandList. */
    GPUPipeline*                    GetPipeline(const GPUPipelineDesc& inDesc,
                                                OnlyCalledBy<GPUGraphicsCommandList>)
                                        { return GetPipelineImpl(inDesc); }

    /** Callback from GPUShader destructor to drop pipelines using the shader. */
    void                            DropPipeline(GPUPipeline* const inPipeline,
                                                 OnlyCalledBy<GPUShader>);

    /**
     * Resource creation methods.
     */

    virtual GPUBufferPtr            CreateBuffer(const GPUBufferDesc& inDesc) = 0;

    GPUPipelinePtr                  CreatePipeline(const GPUPipelineDesc& inDesc);

    virtual GPUResourceViewPtr      CreateResourceView(GPUResource&               inResource,
                                                   const GPUResourceViewDesc& inDesc) = 0;

    virtual GPUShaderPtr            CreateShader(const GPUShaderStage inStage,
                                                 GPUShaderCode        inCode) = 0;

    /**
     * Create and attach a swapchain to the specified window so that it can be
     * rendered to. The swapchain will be permanently associated with the
     * window.
     */
    virtual void                    CreateSwapchain(Window& inWindow) = 0;

    virtual GPUTexturePtr           CreateTexture(const GPUTextureDesc& inDesc) = 0;

    GPUArgumentSetLayout*           GetArgumentSetLayout(GPUArgumentSetLayoutDesc&& inDesc);

protected:
    void                            DestroyResources();

    virtual GPUArgumentSetLayout*   CreateArgumentSetLayoutImpl(GPUArgumentSetLayoutDesc&& inDesc) = 0;
    virtual GPUPipeline*            CreatePipelineImpl(const GPUPipelineDesc& inDesc) = 0;

    virtual void                    EndFrameImpl() = 0;

protected:
    GPUVendor                       mVendor;

    GPUGraphicsContext*             mGraphicsContext;

    GPUUniformPool*                 mUniformPool;

private:
    using ArgumentSetLayoutCache  = HashMap<size_t, GPUArgumentSetLayout*>;
    using PipelineCache           = HashMap<size_t, GPUPipeline*>;

private:
    GPUPipeline*                    GetPipelineImpl(const GPUPipelineDesc& inDesc);

private:
    ArgumentSetLayoutCache          mArgumentSetLayoutCache;
    std::mutex                      mArgumentSetLayoutCacheLock;

    PipelineCache                   mPipelineCache;
    std::shared_mutex               mPipelineCacheLock;

};
