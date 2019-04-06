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

#pragma once

#include "Render/RenderDefs.h"

#include <functional>

class GPUBuffer;
class GPUComputeCommandList;
class GPUGraphicsCommandList;
class GPUResourceView;
class GPUTexture;
class GPUTransferContext;
class RenderGraph;
class RenderGraphContext;

enum RenderGraphPassType
{
    kRenderGraphPassType_Render,
    kRenderGraphPassType_Compute,
    kRenderGraphPassType_Transfer,
};

enum RenderResourceType
{
    kRenderResourceType_Texture,
    kRenderResourceType_Buffer,
};

struct RenderResourceHandle
{
    uint32_t                        index;
};

/**
 * Descriptor for a texture render graph resource. Similar to GPUTextureDesc,
 * except omits usage. Required usage flags are automatically derived by the
 * graph from the requirements of all passes that use the resource.
 */
struct RenderTextureDesc
{
    const char*                     name            = nullptr;

    GPUResourceType                 type            = kGPUResourceType_Texture2D;
    GPUTextureFlags                 flags           = kGPUTexture_None;
    PixelFormat                     format          = kPixelFormat_Unknown;

    uint32_t                        width           = 1;
    uint32_t                        height          = 1;
    uint32_t                        depth           = 1;

    uint16_t                        arraySize       = 1;
    uint8_t                         numMipLevels    = 1;
};

/**
 * Descriptor for a buffer render graph resource. As with RenderTextureDesc,
 * similar to GPUBufferDesc but omits usage.
 */
struct RenderBufferDesc
{
    const char*                     name            = nullptr;

    size_t                          size            = 1;
};

/**
 * Descriptor for a view of a render graph resource. Similar to
 * GPUResourceViewDesc, but instead of a usage flag, specifies a resource
 * state (see below).
 */
struct RenderViewDesc
{
    GPUResourceViewType             type;

    /**
     * Resource state that the subresource range will need to be in when this
     * view is used. Usage flag for the GPUResourceView is derived from this.
     * Must only include states for compatible usages (e.g. same type of read
     * usage in multiple shader stages is allowed).
     */
    GPUResourceState                state;

    PixelFormat                     format          = kPixelFormat_Unknown;

    uint32_t                        mipOffset       = 0;
    uint32_t                        mipCount        = 1;
    uint32_t                        elementOffset   = 0;
    uint32_t                        elementCount    = 1;
};

struct RenderViewHandle
{
    // TODO. Should be simple, it's passed by value.
};

class RenderGraphPass : Uncopyable
{
public:
    using RenderPassFunction      = std::function<void (const RenderGraph&,
                                                        const RenderGraphContext&,
                                                        GPUGraphicsCommandList&)>;

    using ComputePassFunction     = std::function<void (const RenderGraph&,
                                                        const RenderGraphContext&,
                                                        GPUComputeCommandList&)>;

    using TransferPassFunction    = std::function<void (const RenderGraph&,
                                                        const RenderGraphContext&,
                                                        GPUTransferContext&)>;

public:
    /**
     * Set the function for executing the pass. Must use the type appropriate
     * to the type of the pass. The function will be executed on the main
     * thread.
     */
    void                            SetFunction(RenderPassFunction inFunction);
    void                            SetFunction(ComputePassFunction inFunction);
    void                            SetFunction(TransferPassFunction inFunction);

    /**
     * Declare usage of a resource in the pass. This is to be used when the
     * usage does not require a view to be created. When a view is needed, use
     * one of the view creation methods instead.
     *
     * If the specified resource state is writeable, outNewResource must be
     * non-null, where a new handle referring to the resource after the pass
     * will be returned.
     */
    void                            UseResource(const RenderResourceHandle inResource,
                                                const GPUResourceState     inState,
                                                const GPUSubresourceRange& inRange,
                                                RenderResourceHandle*      outNewResource = nullptr);

    /**
     * Create a view to use a resource within the pass. See UseResource()
     * regarding outNewResource.
     */
    RenderViewHandle                CreateView(const RenderResourceHandle inResource,
                                               const RenderViewDesc&      inDesc,
                                               RenderResourceHandle*      outNewResource = nullptr);

    /**
     * For a render pass, creates a view of a resource and sets this as a
     * colour attachment for the pass. State must be
     * kGPUResourceState_RenderTarget, which is a writeable state and therefore
     * outNewResource must be non-null.
     *
     * The render pass load/store ops will be configured automatically by
     * default. If the subresource has no previous writes, the load op will be
     * set to discard, otherwise it will be set to load. Use the Clear*()
     * methods to clear to an explicit value rather than discarding. The store
     * op will be set to discard if no subsequent passes use the resource after
     * the pass, otherwise it will be set to store.
     */
    RenderViewHandle                CreateColourView(const uint8_t              inIndex,
                                                     const RenderResourceHandle inResource,
                                                     const RenderViewDesc&      inDesc,
                                                     RenderResourceHandle*      outNewResource);

    /**
     * For a render pass, creates a view of a resource and sets this as the
     * depth/stencil attachment for the pass. See CreateColourView() regarding
     * load/store ops.
     */
    RenderViewHandle                CreateDepthStencilView(const RenderResourceHandle inResource,
                                                           const RenderViewDesc&      inDesc,
                                                           RenderResourceHandle*      outNewResource = nullptr);

    /**
     * Clear an attachment to a specific value. If the attachment will always
     * be fully overwritten by the pass, do not use this: it will automatically
     * be discarded.
     *
     * It is an error to clear an attachment if the subresource has previous
     * writes, as this means the previous writes are useless. Where a cleared
     * resource is needed, declare a new resource rather than reusing an
     * existing one.
     */
    void                            ClearColour(const uint8_t    inIndex,
                                                const glm::vec4& inValue);
    void                            ClearDepth(const float inValue);
    void                            ClearStencil(const uint32_t inValue);
};

/**
 * Rendering is driven by the render graph. To render the content of a
 * RenderOutput, each RenderLayer registered on it is visited in the defined
 * order to add the render passes (and declare the resources needed by those
 * passes) that they need to produce their output to the graph.
 *
 * Once all passes and resources have been declared (the build phase), the
 * graph determines the execution order of passes based on resource
 * dependencies between passes, and then executes them via the function that
 * was given for the pass (the execute phase). Passes which are determined to
 * not have any contribution to the final output (resources that they write are
 * not consumed) will be removed.
 *
 * During the build phase, we only declare resources and how each pass will
 * use them; no memory is allocated. The real resources are allocated during
 * the execute phase automatically, and can be obtained in the pass function
 * through the graph from the handle that was given out in the build phase.
 *
 * Passes declare their usage of resources, including the resource state that
 * the resource must be in during that pass. Resource state transitions are
 * automatically performed when executing the graph. When a pass writes a
 * resource (it specifies a writeable state for its usage of the resource), a
 * new handle is produced. This handle refers to the content of the resource
 * after the pass has finished executing. Any subsequently added passes which
 * depend on that content must use the new handle.
 *
 * For example, say pass A is added which writes to resource handle 1. The
 * declaration of the write produces resource handle 2. If we subsequently add
 * pass B which reads resource handle 2, then pass B will execute after pass A
 * and read the result it produced. If we subsequently add pass C which reads
 * resource handle 1, then pass C will execute *before* pass A and read the
 * content of the resource before pass A.
 *
 * There can only be one write access to a given resource handle. Read and
 * write to the same resource within a pass is only allowed on non-overlapping
 * subresource ranges (this would allow, for example, a mip generation pass
 * which reads one mip level and writes the one below it).
 */
class RenderGraph : Uncopyable
{
public:
                                    RenderGraph();
                                    ~RenderGraph();

    RenderResourceType              GetResourceType(const RenderResourceHandle inHandle) const;

    /**
     * Graph build methods.
     */

    /**
     * Add a new pass to the graph. The name is for informational purposes,
     * it does not have to be unique but is recommended to be to make it easier
     * to identify passes.
     */
    RenderGraphPass&                AddPass(std::string               inName,
                                            const RenderGraphPassType inType);

    /**
     * Create a new buffer resource. The initial content will be undefined so
     * the first pass that uses it must write to it.
     */
    RenderResourceHandle            CreateBuffer(const RenderBufferDesc& inDesc);

    /**
     * Create a new texture resource. The initial content will be undefined so
     * the first pass that uses it must write to it.
     */
    RenderResourceHandle            CreateTexture(const RenderTextureDesc& inDesc);

    /**
     * Import an external resource into the graph. This is to be used for
     * resources which need to persist outside of the graph, since all
     * resources created within a graph are transient and will be lost after
     * the graph execution completes.
     *
     * A resource state must be specified: it is assumed that the resource is
     * in this state when the graph execution begins, and it will be returned
     * to that state when execution completes.
     *
     * Optional callback functions can be supplied which will be called (from
     * the main thread) before any passes which use the resource are executed,
     * and after all passes have executed. The begin callback will be called
     * before any views to the resource are created.
     */
    RenderResourceHandle            ImportResource(GPUResource* const     inResource,
                                                   const GPUResourceState inState,
                                                   std::function<void ()> inBeginCallback = {},
                                                   std::function<void ()> inEndCallback = {});

    /**
     * Graph execution methods.
     */

    void                            Execute();

    /**
     * Methods to retrieve the real resource from a handle inside a pass
     * function.
     */
    GPUBuffer*                      GetBuffer(const RenderGraphContext&  inContext,
                                              const RenderResourceHandle inHandle);
    GPUTexture*                     GetTexture(const RenderGraphContext&  inContext,
                                               const RenderResourceHandle inHandle);
    GPUResourceView*                GetView(const RenderGraphContext& inContext,
                                            const RenderViewHandle    inHandle);

private:
    struct Resource
    {
        RenderResourceType          type;

        union
        {
            RenderTextureDesc       texture;
            RenderBufferDesc        buffer;
        };
    };

private:
    std::vector<RenderGraphPass*>   mPasses;
    std::vector<Resource>           mResources;

};

inline RenderResourceType RenderGraph::GetResourceType(const RenderResourceHandle inHandle) const
{
    Assert(inHandle.index < mResources.size());
    return mResources[inHandle.index].type;
}

#if 0
void AddExampleComputePass(RenderGraph&          inGraph,
                           RenderResourceHandle& outResult)
{
    struct Resources
    {
        RenderViewHandle    textureView;
    };

    RenderGraphPass& pass = inGraph.AddPass("Test", kRenderGraphPass_Compute);

    Resources resources;

    RenderTextureDesc textureDesc;
    textureDesc.name   = "Example";
    textureDesc.type   = kGPUResourceType_Texture2D;
    textureDesc.format = kPixelFormat_R8G8B8A8;
    textureDesc.width  = 1024;
    textureDesc.height = 1024;

    RenderResourceHandle texture = inGraph.CreateTexture(textureDesc);

    /* Specifies state instead of usage. Derive usage from it. */
    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = kGPUResourceState_ComputeShaderWrite;
    viewDesc.format = textureDesc.format;

    /* Declares the subresource range for writing in the current pass, produces
     * a renamed handle. */
    resources.textureView = pass.CreateView(texture, viewDesc, &outResult);

    pass.SetFunction([resources] (const RenderGraph&        inGraph,
                                  const RenderGraphContext& inContext,
                                  GPUComputeCommandList&    inCmdList)
    {
        /* Context has context information for the current pass that might be
         * needed for the lookup. Would allow multithreading compared to having
         * context information stored in the graph itself. */
        GPUResourceView* const view = inGraph.GetView(inContext, resources.textureView);

        ...
    });
}

void AddExampleInPlaceComputePass(RenderGraph&          inGraph,
                                  RenderResourceHandle& ioTexture)
{
    struct Resources
    {
        RenderViewHandle    textureView;
    };

    RenderGraphPass& pass = inGraph.AddPass("Test", kRenderGraphPass_Compute);

    Resources resources;

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = kGPUResourceState_ComputeShaderWrite;
    viewDesc.format = textureDesc.format;

    resources.textureView = pass.CreateView(ioTexture, viewDesc, &ioTexture);

    pass.SetFunction([resources] (const RenderGraph&        inGraph,
                                  const RenderGraphContext& inContext,
                                  GPUComputeCommandList&    inCmdList)
    {
        GPUResourceView* const view = inGraph.GetView(inContext, resources.textureView);

        ...
    });
}

void AddGBufferPass(RenderGraph&                       inGraph,
                    DeferredRenderPipeline::Resources& outResources)
{
    struct Resources
    {
        RenderViewHandle    GBufferAView;
        RenderViewHandle    GBufferBView;
        RenderViewHandle    GBufferCView;
        RenderViewHandle    GBufferDView;
    };

    RenderGraphPass& pass = inGraph.AddPass("G-Buffer", kRenderGraphPass_Render);

    Resources resources;

    RenderResourceHandle texture;

    RenderTextureDesc textureDesc;
    textureDesc.type   = kGPUResourceType_Texture2D;
    textureDesc.width  = renderTargetWidth;
    textureDesc.height = renderTargetHeight;

    RenderViewDesc viewDesc;
    viewDesc.type  = kGPUResourceViewType_Texture2D;
    viewDesc.state = kGPUResourceState_RenderTarget;

    textureDesc.name       = "G-Buffer A";
    textureDesc.format     = kGBufferAFormat;
    viewDesc.format        = kGBufferAFormat;
    texture                = inGraph.CreateTexture(textureDesc);
    resources.GBufferAView = pass.CreateColourView(0, texture, viewDesc, &outResources.GBufferA);

    textureDesc.name       = "G-Buffer B";
    textureDesc.format     = kGBufferBFormat;
    viewDesc.format        = kGBufferBFormat;
    texture                = inGraph.CreateTexture(textureDesc);
    resources.GBufferBView = pass.CreateColourView(1, texture, viewDesc, &outResources.GBufferB);

    textureDesc.name       = "G-Buffer C";
    textureDesc.format     = kGBufferCFormat;
    viewDesc.format        = kGBufferCFormat;
    texture                = inGraph.CreateTexture(textureDesc);
    resources.GBufferCView = pass.CreateColourView(2, texture, viewDesc, &outResources.GBufferC);

    textureDesc.name       = "G-Buffer D";
    textureDesc.format     = kGBufferDFormat;
    viewDesc.format        = kGBufferDFormat;
    viewDesc.state         = kGPUResourceState_DepthStencilWrite;
    texture                = inGraph.CreateTexture(textureDesc);
    resources.GBufferDView = pass.CreateDepthStencilView(texture, viewDesc, &outResources.GBufferD);

    pass.ClearColour(0, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    pass.ClearColour(1, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    pass.ClearColour(2, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    pass.ClearDepth(1.0f);

    pass.SetFunction([resources] (const RenderGraph&        inGraph,
                                  const RenderGraphContext& inContext,
                                  GPUGraphicsCommandList&   inCmdList)
    {
        ...
    });
}

void AddFXAAPass(RenderGraph&          inGraph,
                 RenderResourceHandle& ioResource)
{
    struct Resources
    {
        RenderViewHandle    shaderView;
    };

    RenderGraphPass& pass = inGraph.AddPass("FXAA", kRenderGraphPass_Render);

    Resources resources;

    /* Sample existing resource. Read-only, does not produce new handle. */
    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.format = texture.GetFormat();
    viewDesc.state  = kGPUResourceState_PixelShaderRead;

    resources.shaderView = pass.CreateView(ioResource, viewDesc)

    /* Write to new resource, exact duplicate of properties of existing
     * resource. */
    RenderTextureDesc textureDesc(ioResource);
    textureDesc.name = "FXAA Output";

    RenderResourceHandle texture = inGraph.CreateTexture(textureDesc);

    viewDesc.state = kGPUResourceState_RenderTarget;

    pass.CreateColourView(0, texture, viewDesc, &ioResource);

    pass.SetFunction([resources] (const RenderGraph&        inGraph,
                                  const RenderGraphContext& inContext,
                                  GPUGraphicsCommandList&   inCmdList)
    {
        GPUResourceView* const view = inGraph.GetView(inContext, resources.shaderView);
        ...
    });
}
#endif