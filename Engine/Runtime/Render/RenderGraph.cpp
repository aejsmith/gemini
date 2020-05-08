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

#include "Render/RenderGraph.h"

#include "Engine/DebugWindow.h"

#include "GPU/GPUBuffer.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPURenderPass.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUTexture.h"
#include "GPU/GPUUtils.h"

#include "Render/RenderLayer.h"
#include "Render/RenderManager.h"
#include "Render/RenderOutput.h"

/**
 * TODO:
 *  - GPU memory aliasing/reuse based on required resource lifetimes.
 *  - Reading depth from shader while bound as depth target doesn't work
 *    currently: have to declare 2 uses, they will conflict. Should combine
 *    them into one use with the union of the states.
 *  - Could add some helper functions for transfer passes for common cases,
 *    e.g. just copying a texture.
 *  - Optimisation of barriers. Initial implementation just does barriers
 *    as needed before each pass during execution, but since we have a view of
 *    the whole frame, we should be able to move them earlier and batch them
 *    together (including using a union of compatible read states if read
 *    by multiple later passes, and potentially use split barriers/events).
 *  - Use FrameAllocator for internal allocations (including STL stuff). Also
 *    could do with a way to get GPU layer objects (resources, views) to be
 *    allocated with it as well.
 *  - We currently do not allow passes to declare usage of a resource version
 *    older than the current: doing so would require the ability to reorder
 *    passes so that the newly added one is executed at the right time to see
 *    the older content. However, this also introduces some ways to declare
 *    impossible scenarios: for example, we could declare pass Z that consumes
 *    resource A version 1 produced by pass X, and resource B version 1
 *    produced by pass Y, but pass Y also produces resource A version 2. Z
 *    needs to execute after Y to see B1, but at that point it would also get
 *    A2 rather than A1. We would need an earlier copy of A1 for Z to use to
 *    resolve it. We would need to detect this situation and either reject it
 *    (require an explicit copy of A) or do a copy internally. For now I'm not
 *    bothering to solve it until we have a use case (if ever).
 *  - Asynchronous compute support.
 *  - Render pass combining. If we have passes that execute consecutively and
 *    have the same render target configuration, combine them into one pass,
 *    which avoids unnecessary store/load between the passes.
 */

class RenderGraphWindow : public DebugWindow
{
public:
                                RenderGraphWindow();

    void                        RenderWindow(const RenderGraph& graph);

    static RenderGraphWindow&   Get();

private:
    RenderGraph::PassKey        mCurrentPass;
    RenderGraph::ResourceKey    mCurrentResource;

    RenderGraph::PassKey        mJumpToPass;
    RenderGraph::ResourceKey    mJumpToResource;
};

RenderGraph::ResourceKey RenderGraph::mDebugOutput;

RenderGraphPass::RenderGraphPass(RenderGraph&              graph,
                                 std::string               name,
                                 const RenderGraphPassType type,
                                 const RenderLayer* const  layer) :
    mGraph      (graph),
    mName       (name),
    mType       (type),
    mLayer      (layer),
    mRequired   (false)
{
}

static GPUResourceUsage ResourceUsageFromState(const GPUResourceState state)
{
    GPUResourceUsage usage = kGPUResourceUsage_Standard;

    if (state & kGPUResourceState_AllShaderRead)
    {
        usage |= kGPUResourceUsage_ShaderRead;
    }

    if (state & kGPUResourceState_AllShaderWrite)
    {
        usage |= kGPUResourceUsage_ShaderWrite;
    }

    if (state & kGPUResourceState_RenderTarget)
    {
        usage |= kGPUResourceUsage_RenderTarget;
    }

    if (state & kGPUResourceState_AllDepthStencil)
    {
        usage |= kGPUResourceUsage_DepthStencil;
    }

    Assert(usage == kGPUResourceUsage_Standard || IsOnlyOneBitSet(usage));

    return usage;
}

void RenderGraphPass::UseResource(const RenderResourceHandle handle,
                                  const GPUSubresourceRange& range,
                                  const GPUResourceState     state,
                                  RenderResourceHandle*      outNewHandle)
{
    RenderGraph::Resource* resource = mGraph.mResources[handle.index];

    const bool isWrite = state & kGPUResourceState_AllWrite;

    AssertMsg(isWrite || !outNewHandle,
              "outNewHandle must be null for a read-only access");
    AssertMsg(resource->currentVersion == handle.version,
              "Resource access must be to current version (see TODO)");

    GPUUtils::ValidateResourceState(state, resource->type == kRenderResourceType_Texture);

    bool needSplitState = false;

    for (RenderGraphPass::UsedResource& otherUse : mUsedResources)
    {
        if (otherUse.handle.index == handle.index)
        {
            AssertMsg(!otherUse.range.Overlaps(range),
                      "Subresources cannot be used multiple times in the same pass");

            /* If we have uses of multiple different subresources in this
             * resource with conflicting states, we'll need split state
             * tracking. */
            if (otherUse.state != state)
            {
                otherUse.needSplitState = true;
                needSplitState          = true;
            }
        }
    }

    mUsedResources.emplace_back();
    UsedResource& use  = mUsedResources.back();
    use.handle         = handle;
    use.range          = range;
    use.state          = state;
    use.needSplitState = needSplitState;

    /* Add required usage flags for this resource state. */
    resource->usage |= ResourceUsageFromState(state);

    if (isWrite)
    {
        resource->currentVersion++;

        Assert(resource->producers.size() == resource->currentVersion);

        resource->producers.emplace_back(this);

        if (outNewHandle)
        {
            outNewHandle->index   = handle.index;
            outNewHandle->version = resource->currentVersion;
        }
    }
}

RenderViewHandle RenderGraphPass::CreateView(const RenderResourceHandle handle,
                                             const RenderViewDesc&      desc,
                                             RenderResourceHandle*      outNewHandle)
{
    const bool isTexture = mGraph.GetResourceType(handle) == kRenderResourceType_Texture;

    GPUSubresourceRange range = { 0, 1, 0, 1 };

    if (isTexture)
    {
        range.mipOffset   = desc.mipOffset;
        range.mipCount    = desc.mipCount;
        range.layerOffset = desc.elementOffset;
        range.layerCount  = desc.elementCount;
    }
    else
    {
        Assert(desc.mipOffset == 0 && desc.mipCount == 1);
    }

    UseResource(handle,
                range,
                desc.state,
                outNewHandle);

    RenderViewHandle viewHandle;
    viewHandle.pass  = this;
    viewHandle.index = mViews.size();

    mViews.emplace_back();
    View& view = mViews.back();
    view.resource = handle;
    view.desc     = desc;
    view.view     = nullptr;

    if (isTexture && view.desc.format == kPixelFormat_Unknown)
    {
        /* Set from texture format. */
        view.desc.format = mGraph.GetTextureDesc(handle).format;
    }

    return viewHandle;
}

void RenderGraphPass::SetColour(const uint8_t              index,
                                const RenderResourceHandle handle,
                                RenderResourceHandle*      outNewHandle)
{
    const RenderTextureDesc& textureDesc = mGraph.GetTextureDesc(handle);

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = kGPUResourceState_RenderTarget;
    viewDesc.format = textureDesc.format;

    SetColour(index, handle, viewDesc, outNewHandle);
}

void RenderGraphPass::SetColour(const uint8_t              index,
                                const RenderResourceHandle handle,
                                const RenderViewDesc&      desc,
                                RenderResourceHandle*      outNewHandle)
{
    Assert(mType == kRenderGraphPassType_Render);
    Assert(index < kMaxRenderPassColourAttachments);
    Assert(mGraph.GetResourceType(handle) == kRenderResourceType_Texture);
    Assert(desc.state == kGPUResourceState_RenderTarget);
    Assert(PixelFormatInfo::IsColour(desc.format));
    Assert(outNewHandle);

    Attachment& attachment = mColour[index];
    attachment.view = CreateView(handle, desc, outNewHandle);

    /* If this is the first version of the resource, it will be cleared, so set
     * a default clear value. */
    attachment.clearData.type   = GPUTextureClearData::kColour;
    attachment.clearData.colour = glm::vec4(0.0f);
}

void RenderGraphPass::SetDepthStencil(const RenderResourceHandle handle,
                                      const GPUResourceState     state,
                                      RenderResourceHandle*      outNewHandle)
{
    const RenderTextureDesc& textureDesc = mGraph.GetTextureDesc(handle);

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = state;
    viewDesc.format = textureDesc.format;

    SetDepthStencil(handle, viewDesc, outNewHandle);
}

void RenderGraphPass::SetDepthStencil(const RenderResourceHandle handle,
                                      const RenderViewDesc&      desc,
                                      RenderResourceHandle*      outNewHandle)
{
    Assert(mType == kRenderGraphPassType_Render);
    Assert(mGraph.GetResourceType(handle) == kRenderResourceType_Texture);
    Assert(desc.state & kGPUResourceState_AllDepthStencil && IsOnlyOneBitSet(desc.state));
    Assert(PixelFormatInfo::IsDepth(desc.format));

    mDepthStencil.view = CreateView(handle, desc, outNewHandle);

    /* If this is the first version of the resource, it will be cleared, so set
     * a default clear value. */
    mDepthStencil.clearData.type    = (PixelFormatInfo::IsDepthStencil(desc.format))
                                          ? GPUTextureClearData::kDepthStencil
                                          : GPUTextureClearData::kDepth;
    mDepthStencil.clearData.depth   = 1.0f;
    mDepthStencil.clearData.stencil = 0;
}

void RenderGraphPass::ClearColour(const uint8_t    index,
                                  const glm::vec4& value)
{
    Assert(index < kMaxRenderPassColourAttachments);

    Attachment& attachment = mColour[index];
    Assert(attachment.view);

    View& view = mViews[attachment.view.index];
    Assert(view.resource.version == 0);
    Unused(view);

    attachment.clearData.colour = value;
}

void RenderGraphPass::ClearDepth(const float value)
{
    Assert(mDepthStencil.view);

    View& view = mViews[mDepthStencil.view.index];
    Assert(view.resource.version == 0);
    Unused(view);

    mDepthStencil.clearData.depth = value;
}

void RenderGraphPass::ClearStencil(const uint32_t value)
{
    Assert(mDepthStencil.view);

    View& view = mViews[mDepthStencil.view.index];
    Assert(view.resource.version == 0);
    Unused(view);

    mDepthStencil.clearData.stencil = value;
}

GPUResourceView* RenderGraphPass::GetView(const RenderViewHandle handle) const
{
    Assert(handle.pass == this);
    Assert(mGraph.mIsExecuting);
    Assert(handle.index < mViews.size());
    AssertMsg(mViews[handle.index].view, "Attempt to use view of culled resource");

    return mViews[handle.index].view;
}

RenderGraph::RenderGraph() :
    mIsExecuting    (false)
{
}

RenderGraph::~RenderGraph()
{
    for (auto pass : mPasses)
    {
        delete pass;
    }

    for (auto resource : mResources)
    {
        delete resource;
    }
}

RenderGraphPass& RenderGraph::AddPass(std::string               name,
                                      const RenderGraphPassType type)
{
    RenderGraphPass* pass = new RenderGraphPass(*this,
                                                std::move(name),
                                                type,
                                                mCurrentLayer);

    mPasses.emplace_back(pass);

    return *pass;
}

RenderGraphPass& RenderGraph::AddBlitPass(std::string                name,
                                          const RenderResourceHandle destHandle,
                                          const GPUSubresource       destSubresource,
                                          const RenderResourceHandle sourceHandle,
                                          const GPUSubresource       sourceSubresource,
                                          RenderResourceHandle*      outNewHandle)
{
    Assert(GetResourceType(destHandle) == kRenderResourceType_Texture);
    Assert(GetResourceType(sourceHandle) == kRenderResourceType_Texture);
    Assert(outNewHandle);

    RenderGraphPass& pass = AddPass(std::move(name), kRenderGraphPassType_Transfer);

    pass.UseResource(sourceHandle,
                     sourceSubresource,
                     kGPUResourceState_TransferRead,
                     nullptr);

    pass.UseResource(destHandle,
                     destSubresource,
                     kGPUResourceState_TransferWrite,
                     outNewHandle);

    pass.SetFunction([=] (const RenderGraph&     graph,
                          const RenderGraphPass& pass,
                          GPUTransferContext&    context)
    {
        context.BlitTexture(graph.GetTexture(destHandle),
                            destSubresource,
                            graph.GetTexture(sourceHandle),
                            sourceSubresource);
    });

    return pass;
}

RenderGraphPass& RenderGraph::AddUploadPass(std::string                name,
                                            const RenderResourceHandle destHandle,
                                            const uint32_t             destOffset,
                                            GPUStagingBuffer           sourceBuffer,
                                            RenderResourceHandle*      outNewHandle)
{
    Assert(GetResourceType(destHandle) == kRenderResourceType_Buffer);
    Assert(outNewHandle);

    RenderGraphPass& pass = AddPass(std::move(name), kRenderGraphPassType_Transfer);

    pass.UseResource(destHandle,
                     GPUSubresource{0, 0},
                     kGPUResourceState_TransferWrite,
                     outNewHandle);

    /* Ugh, non-copyable objects can't be captured in a std::function since
     * that is copyable. Make a transient allocation to hold the staging handle
     * instead. */
    auto sourceCopy = NewTransient<GPUStagingBuffer>(std::move(sourceBuffer));

    pass.SetFunction([destHandle, destOffset, sourceCopy]
                     (const RenderGraph&     graph,
                      const RenderGraphPass& pass,
                      GPUTransferContext&    context)
    {
        context.UploadBuffer(graph.GetBuffer(destHandle),
                             *sourceCopy,
                             sourceCopy->GetSize(),
                             destOffset,
                             0);
    });

    return pass;
}

RenderGraph::Resource::Resource() :
    texture         (),
    usage           (kGPUResourceUsage_Standard),
    currentVersion  (0),
    originalState   (kGPUResourceState_None),
    output          (nullptr),
    imported        (false),
    required        (false),
    begun           (false),
    firstPass       (nullptr),
    lastPass        (nullptr),
    resource        (nullptr),
    currentState    (kGPUResourceState_None),
    debugResource   (nullptr)
{
    /* Nothing produced the initial version. */
    producers.emplace_back(nullptr);
}

const char* RenderGraph::Resource::GetName() const
{
    return (this->type == kRenderResourceType_Texture)
               ? this->texture.name
               : this->buffer.name;
}

RenderResourceHandle RenderGraph::CreateBuffer(const RenderBufferDesc& desc)
{
    Resource* resource = new Resource;
    resource->type     = kRenderResourceType_Buffer;
    resource->layer    = mCurrentLayer;
    resource->buffer   = desc;

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = resource->currentVersion;

    mResources.emplace_back(resource);

    return handle;
}

RenderResourceHandle RenderGraph::CreateTexture(const RenderTextureDesc& desc)
{
    Resource* resource = new Resource;
    resource->layer    = mCurrentLayer;
    resource->type     = kRenderResourceType_Texture;
    resource->texture  = desc;

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = resource->currentVersion;

    mResources.emplace_back(resource);

    return handle;
}

RenderResourceHandle RenderGraph::ImportResource(GPUResource* const        extResource,
                                                 const GPUResourceState    state,
                                                 const char* const         name,
                                                 std::function<void ()>    beginCallback,
                                                 std::function<void ()>    endCallback,
                                                 const RenderOutput* const output)
{
    Resource* resource      = new Resource;
    resource->layer         = nullptr;
    resource->imported      = true;
    resource->resource      = extResource;
    resource->originalState = state;
    resource->currentState  = state;
    resource->output        = output;
    resource->beginCallback = std::move(beginCallback);
    resource->endCallback   = std::move(endCallback);

    if (extResource->IsTexture())
    {
        resource->type = kRenderResourceType_Texture;

        const auto texture      = static_cast<const GPUTexture*>(extResource);
        RenderTextureDesc& desc = resource->texture;

        desc.name         = name;
        desc.type         = texture->GetType();
        desc.flags        = texture->GetFlags();
        desc.format       = texture->GetFormat();
        desc.width        = texture->GetWidth();
        desc.height       = texture->GetHeight();
        desc.depth        = texture->GetDepth();
        desc.arraySize    = texture->GetArraySize();
        desc.numMipLevels = texture->GetNumMipLevels();
    }
    else
    {
        resource->type = kRenderResourceType_Buffer;

        const auto buffer      = static_cast<const GPUBuffer*>(extResource);
        RenderBufferDesc& desc = resource->buffer;

        desc.name = name;
        desc.size = buffer->GetSize();
    }

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = resource->currentVersion;

    mResources.emplace_back(resource);

    return handle;
}

void RenderGraph::TransitionResource(Resource&                  resource,
                                     const GPUSubresourceRange& range,
                                     const GPUResourceState     state,
                                     const bool                 needSplitState)
{
    GPUSubresourceRange transitionRange = range;

    const bool isWholeResource = range == resource.resource->GetSubresourceRange();

    if (!isWholeResource)
    {
        /* When different subresources are being used with different states by
         * the same pass, we need to use split state tracking for individual
         * subresources. Otherwise, we'll just treat the whole resource as one
         * where we can. */
        if (needSplitState)
        {
            Fatal("TODO: Per-subresource state tracking");
        }
        else
        {
            transitionRange = resource.resource->GetSubresourceRange();
        }
    }

    if (resource.currentState != state)
    {
        mBarriers.emplace_back();
        GPUResourceBarrier& barrier = mBarriers.back();
        barrier.resource     = resource.resource;
        barrier.range        = transitionRange;
        barrier.currentState = resource.currentState;
        barrier.newState     = state;

        /* Discard if state is currently none, i.e. this is first use. */
        barrier.discard = resource.currentState == kGPUResourceState_None;

        resource.currentState = state;
    }
}

void RenderGraph::FlushBarriers()
{
    if (!mBarriers.empty())
    {
        GPUGraphicsContext::Get().ResourceBarrier(mBarriers.data(), mBarriers.size());
        mBarriers.clear();
    }
}

void RenderGraph::DetermineRequiredPasses()
{
    /*
     * This function determines which passes are actually required to produce
     * the final outputs of the graph. The final outputs are all imported
     * resources that are written by any graph pass.
     *
     * Therefore, we will need the passes that write the final version of each
     * imported resource to be executed. We then work back from there, and mark
     * the passes that produce each of their dependencies as required, and so
     * on.
     */

    RenderGraphPass** const passes = AllocateStackArray(RenderGraphPass*, mPasses.size());
    uint16_t passCount             = 0;

    /* Get the passes forced to be executed. */
    for (RenderGraphPass* pass : mPasses)
    {
        if (pass->mRequired)
        {
            passes[passCount++] = pass;
        }
    }

    /* Get the passes producing imported resources. */
    for (const Resource* const resource : mResources)
    {
        if (resource->imported && resource->currentVersion > 0)
        {
            RenderGraphPass* pass = resource->producers[resource->currentVersion];
            if (!pass->mRequired)
            {
                pass->mRequired = true;
                passes[passCount++] = pass;
            }
        }
    }

    while (passCount > 0)
    {
        RenderGraphPass* const pass = passes[--passCount];

        for (const RenderGraphPass::UsedResource& use : pass->mUsedResources)
        {
            Resource* const resource = mResources[use.handle.index];

            resource->required = true;

            RenderGraphPass* const producer = resource->producers[use.handle.version];
            Assert(producer || use.handle.version == 0);

            /* Don't revisit passes we've already been to. */
            if (producer && !producer->mRequired)
            {
                producer->mRequired = true;
                passes[passCount++] = producer;
            }
        }
    }

    /* Set the first and last required pass using each resource. Passes are in
     * execution order. */
    for (RenderGraphPass* pass : mPasses)
    {
        if (pass->mRequired)
        {
            for (const RenderGraphPass::UsedResource& use : pass->mUsedResources)
            {
                Resource* const resource = mResources[use.handle.index];

                if (!resource->firstPass)
                {
                    Assert(use.handle.version == 0);
                    resource->firstPass = pass;
                }

                resource->lastPass = pass;
            }
        }
    }
}

void RenderGraph::MakeBufferDesc(const Resource* const resource,
                                 GPUBufferDesc&        outDesc)
{
    outDesc.usage = resource->usage;
    outDesc.size  = resource->buffer.size;
}

void RenderGraph::MakeTextureDesc(const Resource* const resource,
                                  GPUTextureDesc&       outDesc)
{
    outDesc.type         = resource->texture.type;
    outDesc.usage        = resource->usage;
    outDesc.flags        = resource->texture.flags;
    outDesc.format       = resource->texture.format;
    outDesc.width        = resource->texture.width;
    outDesc.height       = resource->texture.height;
    outDesc.depth        = resource->texture.depth;
    outDesc.arraySize    = resource->texture.arraySize;
    outDesc.numMipLevels = resource->texture.numMipLevels;
}

void RenderGraph::AllocateResources()
{
    for (Resource* resource : mResources)
    {
        if (!resource->required || resource->imported)
        {
            continue;
        }

        switch (resource->type)
        {
            case kRenderResourceType_Buffer:
            {
                GPUBufferDesc desc;
                MakeBufferDesc(resource, desc);

                resource->resource = RenderManager::Get().GetTransientBuffer(desc, {});

                if (GEMINI_GPU_MARKERS && resource->buffer.name)
                {
                    resource->resource->SetName(resource->buffer.name);
                }

                break;
            }

            case kRenderResourceType_Texture:
            {
                GPUTextureDesc desc;
                MakeTextureDesc(resource, desc);

                resource->resource = RenderManager::Get().GetTransientTexture(desc, {});

                if (GEMINI_GPU_MARKERS && resource->texture.name)
                {
                    resource->resource->SetName(resource->texture.name);
                }

                break;
            }

            default:
            {
                Unreachable();
                break;
            }
        }
    }
}

void RenderGraph::EndResources()
{
    const Resource* const debugResource = FindResource(mDebugOutput);
    if (mDebugOutput.IsValid() && !debugResource)
    {
        /* Clear debug resource key if it is no longer present. */
        mDebugOutput = ResourceKey();
    }

    /* Transition imported resources back to the original state. */
    for (Resource* resource : mResources)
    {
        if (resource->begun && resource->imported)
        {
            /* If there is currently a debug output resource, blit it onto the
             * RenderOutput that it was created within (before we transition
             * the output's resource to its final state). */
            if (debugResource && debugResource->layer->GetLayerOutput() == resource->output)
            {
                TransitionResource(*resource,
                                   resource->resource->GetSubresourceRange(),
                                   kGPUResourceState_TransferWrite);
                FlushBarriers();

                GPUGraphicsContext::Get().BlitTexture(static_cast<GPUTexture*>(resource->resource),
                                                      { 0, 0 },
                                                      static_cast<GPUTexture*>(debugResource->debugResource),
                                                      { 0, 0 });
            }

            TransitionResource(*resource,
                               resource->resource->GetSubresourceRange(),
                               resource->originalState);
        }
    }

    /* Flush those barriers. This may need to be done before end callbacks. */
    FlushBarriers();

    for (Resource* resource : mResources)
    {
        if (resource->begun)
        {
            if (resource->endCallback)
            {
                resource->endCallback();
            }
        }
    }
}

void RenderGraph::PrepareResources(RenderGraphPass& pass)
{
    for (const RenderGraphPass::UsedResource& use : pass.mUsedResources)
    {
        Resource* const resource = mResources[use.handle.index];

        /* If this is the first pass to use the resource and it has a begin
         * callback, call that now. Could have multiple uses of a resource
         * within the pass, only begin once. */
        if (resource->firstPass == &pass && !resource->begun)
        {
            if (resource->beginCallback)
            {
                resource->beginCallback();
            }

            resource->begun = true;
        }

        TransitionResource(*resource,
                           use.range,
                           use.state,
                           use.needSplitState);
    }

    FlushBarriers();
}

void RenderGraph::CreateViews(RenderGraphPass& pass)
{
    for (RenderGraphPass::View& view : pass.mViews)
    {
        Resource* const resource = mResources[view.resource.index];

        if (!resource->required)
        {
            continue;
        }

        GPUResourceViewDesc desc;
        desc.type          = view.desc.type;
        desc.usage         = ResourceUsageFromState(view.desc.state);
        desc.format        = view.desc.format;
        desc.mipOffset     = view.desc.mipOffset;
        desc.mipCount      = view.desc.mipCount;
        desc.elementOffset = view.desc.elementOffset;
        desc.elementCount  = view.desc.elementCount;

        view.view = GPUDevice::Get().CreateResourceView(resource->resource, desc);
    }
}

void RenderGraph::DestroyViews(RenderGraphPass& pass)
{
    for (RenderGraphPass::View& view : pass.mViews)
    {
        delete view.view;
        view.view = nullptr;
    }
}

void RenderGraph::ExecutePass(RenderGraphPass& pass)
{
    /* The usual PROFILER_SCOPE macros store the token in a static local, which
     * won't work for a dynamic name string, so do this manually. */
    #if GEMINI_PROFILER
        const MicroProfileToken token = MicroProfileGetToken(RENDER_PROFILER_NAME,
                                                             pass.mName.c_str(),
                                                             RENDER_PROFILER_COLOUR,
                                                             MicroProfileTokenTypeCpu);

        MicroProfileScopeHandler profileScope(token);
    #endif

    switch (pass.mType)
    {
        case kRenderGraphPassType_Render:
        {
            Assert(pass.mRenderFunction);

            GPUGraphicsContext& context = GPUGraphicsContext::Get();

            GPURenderPass renderPass;

            for (size_t i = 0; i < kMaxRenderPassColourAttachments; i++)
            {
                RenderGraphPass::Attachment& colourAtt = pass.mColour[i];

                if (colourAtt.view)
                {
                    RenderGraphPass::View& view = pass.mViews[colourAtt.view.index];

                    renderPass.SetColour(i, view.view);

                    /* If this is the first pass to use the resource, clear it.
                     * If it is the last, discard it, unless it is an imported
                     * resource. TODO: Wouldn't always want to clear imported
                     * resources, but do sometimes. */
                    Resource* const resource = mResources[view.resource.index];

                    if (resource->firstPass == &pass)
                    {
                        renderPass.ClearColour(i, colourAtt.clearData.colour);
                    }

                    if (!resource->imported && resource->lastPass == &pass)
                    {
                        renderPass.DiscardColour(i);
                    }
                }
            }

            RenderGraphPass::Attachment& depthAtt = pass.mDepthStencil;

            if (depthAtt.view)
            {
                RenderGraphPass::View& view = pass.mViews[depthAtt.view.index];

                renderPass.SetDepthStencil(view.view);

                Resource* const resource = mResources[view.resource.index];

                if (resource->firstPass == &pass)
                {
                    renderPass.ClearDepth(depthAtt.clearData.depth);

                    if (PixelFormatInfo::IsDepthStencil(resource->texture.format))
                    {
                        renderPass.ClearStencil(depthAtt.clearData.stencil);
                    }
                }

                if (!resource->imported && resource->lastPass == &pass)
                {
                    renderPass.DiscardDepth();

                    if (PixelFormatInfo::IsDepthStencil(resource->texture.format))
                    {
                        renderPass.DiscardStencil();
                    }
                }
            }

            GPUGraphicsCommandList* const cmdList = context.CreateRenderPass(renderPass);
            cmdList->Begin();

            pass.mRenderFunction(*this, pass, *cmdList);

            cmdList->End();

            GPU_MARKER_SCOPE(context, pass.mName);
            context.SubmitRenderPass(cmdList);

            break;
        }

        case kRenderGraphPassType_Compute:
        {
            Assert(pass.mComputeFunction);

            /* TODO: Async compute. */
            GPUComputeContext& context = GPUGraphicsContext::Get();

            GPUComputeCommandList* const cmdList = context.CreateComputePass();
            cmdList->Begin();

            pass.mComputeFunction(*this, pass, *cmdList);

            cmdList->End();

            GPU_MARKER_SCOPE(context, pass.mName);
            context.SubmitComputePass(cmdList);

            break;
        }

        case kRenderGraphPassType_Transfer:
        {
            Assert(pass.mTransferFunction);

            /* Transfer passes are just executed on the main graphics context.
             * Not worth using a transfer queue for mid-frame transfers, it'll
             * just add synchronisation overhead.
             *
             * TODO: Any use case for doing transfers on the async compute
             * queue, i.e. between async compute passes?
             *
             * TODO: Could do transfers to resources with no previous use in
             * the frame on the transfer queue? Could potentially overlap with
             * end of previous frame. */
            GPUComputeContext& context = GPUGraphicsContext::Get();

            GPU_MARKER_SCOPE(context, pass.mName);
            pass.mTransferFunction(*this, pass, context);

            break;
        }

        default:
        {
            Unreachable();
            break;
        }
    }

    if (mDebugOutput.IsValid())
    {
        /* Check if this pass produces the resource version we want as the debug
         * output, and if so, copy it. */
        for (const RenderGraphPass::UsedResource& use : pass.mUsedResources)
        {
            RenderGraph::Resource* resource = mResources[use.handle.index];

            if (resource->layer == mDebugOutput.layer &&
                resource->GetName() &&
                strcmp(resource->GetName(), mDebugOutput.name) == 0 &&
                pass.mName == mDebugOutput.versionProducer)
            {
                Assert(!resource->debugResource);
                Assert(resource->type == kRenderResourceType_Texture);

                /* TODO: Depth textures - we'll need a shader-based copy to a
                 * colour format texture. */
                GPUTextureDesc desc;
                MakeTextureDesc(resource, desc);

                resource->debugResource = RenderManager::Get().GetTransientTexture(desc, {});

                GPUTexture* const texture      = static_cast<GPUTexture*>(resource->resource);
                GPUTexture* const debugTexture = static_cast<GPUTexture*>(resource->debugResource);

                GPUTransferContext& context = GPUGraphicsContext::Get();
                context.ResourceBarrier(texture, resource->currentState, kGPUResourceState_TransferRead);
                context.ResourceBarrier(debugTexture, kGPUResourceState_None, kGPUResourceState_TransferWrite);
                context.BlitTexture(debugTexture, { 0, 0 }, texture, { 0, 0 });
                context.ResourceBarrier(texture, kGPUResourceState_TransferRead, resource->currentState);
                context.ResourceBarrier(debugTexture, kGPUResourceState_TransferWrite, kGPUResourceState_TransferRead);

                break;
            }
        }
    }
}

void RenderGraph::Execute()
{
    RENDER_PROFILER_FUNC_SCOPE();

    mIsExecuting = true;

    DetermineRequiredPasses();
    AllocateResources();

    for (RenderGraphPass* pass : mPasses)
    {
        if (pass->mRequired)
        {
            PrepareResources(*pass);
            CreateViews(*pass);

            ExecutePass(*pass);

            DestroyViews(*pass);
        }
    }

    EndResources();

    for (const Destructor& destructor : mDestructors)
    {
        destructor();
    }

    /* Our state is transient so we render the window manually here. */
    RenderGraphWindow::Get().RenderWindow(*this);
}

GPUBuffer* RenderGraph::GetBuffer(const RenderResourceHandle handle) const
{
    Assert(mIsExecuting);
    Assert(GetResourceType(handle) == kRenderResourceType_Buffer);
    AssertMsg(mResources[handle.index]->resource, "Attempt to use culled resource");

    return static_cast<GPUBuffer*>(mResources[handle.index]->resource);
}

GPUTexture* RenderGraph::GetTexture(const RenderResourceHandle handle) const
{
    Assert(mIsExecuting);
    Assert(GetResourceType(handle) == kRenderResourceType_Texture);
    AssertMsg(mResources[handle.index]->resource, "Attempt to use culled resource");

    return static_cast<GPUTexture*>(mResources[handle.index]->resource);
}

void RenderGraph::AddDestructor(Destructor destructor)
{
    mDestructors.emplace_back(std::move(destructor));
}

const RenderGraphPass* RenderGraph::FindPass(const PassKey& key) const
{
    if (!key.name.empty())
    {
        for (const RenderGraphPass* pass : mPasses)
        {
            if (pass->mLayer == key.layer && pass->mName == key.name)
            {
                return pass;
            }
        }
    }

    return nullptr;
}

const RenderGraph::Resource* RenderGraph::FindResource(const ResourceKey& key) const
{
    if (key.name)
    {
        for (const Resource* resource : mResources)
        {
            if (resource->layer == key.layer &&
                resource->GetName() &&
                strcmp(resource->GetName(), key.name) == 0)
            {
                return resource;
            }
        }
    }

    return nullptr;
}

/**
 * Debug window implementation.
 */

RenderGraphWindow::RenderGraphWindow() :
    DebugWindow ("Render", "Render Graph")
{
}

RenderGraphWindow& RenderGraphWindow::Get()
{
    static RenderGraphWindow sWindow;
    return sWindow;
}

void RenderGraphWindow::RenderWindow(const RenderGraph& graph)
{
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(350, 600), ImGuiCond_Once);

    if (!Begin())
    {
        return;
    }

    if (!ImGui::BeginTabBar("##TabBar"))
    {
        ImGui::End();
        return;
    }

    constexpr ImGuiTreeNodeFlags kNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                              ImGuiTreeNodeFlags_DefaultOpen;
    constexpr ImGuiTreeNodeFlags kLeafFlags = ImGuiTreeNodeFlags_Leaf |
                                              ImGuiTreeNodeFlags_NoTreePushOnOpen;

    auto SmallButton = [] (const char* const label, const uint32_t width)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const float padding = style.FramePadding.y;
        style.FramePadding.y = 0.0f;
        const bool pressed = ImGui::Button(label, ImVec2(width, 0));
        style.FramePadding.y = padding;
        return pressed;
    };

    auto GetPassKey = [] (const RenderGraphPass* pass) -> RenderGraph::PassKey
    {
        RenderGraph::PassKey key;

        if (pass)
        {
            key.layer = pass->mLayer;
            key.name  = pass->mName;
        }

        return key;
    };

    auto GetResourceKey = [] (const RenderGraph::Resource* resource) -> RenderGraph::ResourceKey
    {
        RenderGraph::ResourceKey key;

        if (resource)
        {
            key.layer = resource->layer;
            key.name  = resource->GetName();
        }

        return key;
    };

    bool selectPasses    = false;
    bool selectResources = false;

    {
        const RenderGraphPass* jumpToPass           = graph.FindPass(mJumpToPass);
        const RenderGraph::Resource* jumpToResource = graph.FindResource(mJumpToResource);

        if (jumpToPass)
        {
            selectPasses = true;
            mCurrentPass = mJumpToPass;
        }
        else if (jumpToResource)
        {
            selectResources  = true;
            mCurrentResource = mJumpToResource;
        }

        mJumpToPass     = RenderGraph::PassKey();
        mJumpToResource = RenderGraph::ResourceKey();
    }

    if (ImGui::BeginTabItem("Passes", nullptr, (selectPasses) ? ImGuiTabItemFlags_SetSelected : 0))
    {
        const RenderGraphPass* currentPass = graph.FindPass(mCurrentPass);

        /* Tree of all outputs/layers/passes. */
        {
            ImGui::BeginChild("PassTree",
                              ImVec2(0, ImGui::GetContentRegionAvail().y * 0.4f),
                              false);

            for (const RenderOutput* output : RenderManager::Get().GetOutputs())
            {
                if (!ImGui::TreeNodeEx(output, kNodeFlags, "%s", output->GetName().c_str()))
                {
                    continue;
                }

                for (const RenderLayer* layer : output->GetLayers())
                {
                    if (!ImGui::TreeNodeEx(output, kNodeFlags, "%s", layer->GetName().c_str()))
                    {
                        continue;
                    }

                    for (const RenderGraphPass* pass : graph.mPasses)
                    {
                        if (pass->mLayer != layer)
                        {
                            continue;
                        }

                        ImGuiTreeNodeFlags flags = kLeafFlags;
                        if (pass == currentPass)
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::TreeNodeEx(pass->mName.c_str(), flags, "%s", pass->mName.c_str());

                        if (ImGui::IsItemClicked())
                        {
                            currentPass  = pass;
                            mCurrentPass = GetPassKey(pass);
                        }
                    }

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            ImGui::EndChild();
        }

        ImGui::Separator();
        ImGui::Spacing();

        /* Information about the pass. */
        if (currentPass)
        {
            const char* typeStr = "";
            switch (currentPass->mType)
            {
                case kRenderGraphPassType_Render:   typeStr = "Render"; break;
                case kRenderGraphPassType_Compute:  typeStr = "Compute"; break;
                case kRenderGraphPassType_Transfer: typeStr = "Transfer"; break;
            }

            ImGui::Text("Type:     %s", typeStr);
            ImGui::Text("Required: %s", (currentPass->mRequired) ? "Yes" : "No");

            ImGui::NewLine();

            ImGui::Text("Inputs:");
            ImGui::PushID("InputTree");

            for (const RenderGraphPass::UsedResource& use : currentPass->mUsedResources)
            {
                const RenderGraph::Resource* resource = graph.mResources[use.handle.index];

                /* Don't list as an input if this is the first producer. */
                if (resource->imported || use.handle.version != 0)
                {
                    ImGui::TreeNodeEx(resource->GetName(),
                                      kLeafFlags | ImGuiTreeNodeFlags_Bullet,
                                      "%s (version %u)",
                                      resource->GetName(),
                                      use.handle.version);

                    if (ImGui::IsItemClicked())
                    {
                        mJumpToResource = GetResourceKey(resource);
                    }
                }
            }

            ImGui::PopID();
            ImGui::NewLine();

            ImGui::Text("Outputs:");
            ImGui::PushID("OutputTree");

            for (const RenderGraphPass::UsedResource& use : currentPass->mUsedResources)
            {
                const RenderGraph::Resource* resource = graph.mResources[use.handle.index];

                if (use.state & kGPUResourceState_AllWrite)
                {
                    ImGui::TreeNodeEx(resource->GetName(),
                                      kLeafFlags | ImGuiTreeNodeFlags_Bullet,
                                      "%s (version %u)",
                                      resource->GetName(),
                                      use.handle.version + 1);

                    if (ImGui::IsItemClicked())
                    {
                        mJumpToResource = GetResourceKey(resource);
                    }
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Resources", nullptr, (selectResources) ? ImGuiTabItemFlags_SetSelected : 0))
    {
        const RenderGraph::Resource* currentResource = graph.FindResource(mCurrentResource);

        /* Tree of all outputs/layers/resources. */
        {
            ImGui::BeginChild("ResourceTree",
                              ImVec2(0, ImGui::GetContentRegionAvail().y * 0.4f),
                              false);

            for (const RenderOutput* output : RenderManager::Get().GetOutputs())
            {
                if (!ImGui::TreeNodeEx(output, kNodeFlags, "%s", output->GetName().c_str()))
                {
                    continue;
                }

                for (const RenderLayer* layer : output->GetLayers())
                {
                    if (!ImGui::TreeNodeEx(output, kNodeFlags, "%s", layer->GetName().c_str()))
                    {
                        continue;
                    }

                    for (const RenderGraph::Resource* resource : graph.mResources)
                    {
                        if (resource->layer != layer)
                        {
                            continue;
                        }

                        ImGuiTreeNodeFlags flags = kLeafFlags;
                        if (resource == currentResource)
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::TreeNodeEx(resource->GetName(), flags, "%s", resource->GetName());

                        if (ImGui::IsItemClicked())
                        {
                            currentResource  = resource;
                            mCurrentResource = GetResourceKey(resource);
                        }
                    }

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            ImGui::EndChild();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Debug Output:");
        ImGui::SameLine();

        if (graph.FindResource(RenderGraph::mDebugOutput))
        {
            ImGui::Text("%s (%s)",
                        RenderGraph::mDebugOutput.name,
                        RenderGraph::mDebugOutput.versionProducer.c_str());
        }
        else
        {
            ImGui::Text("None");
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 60);
        if (SmallButton("Clear", 50))
        {
            RenderGraph::mDebugOutput = RenderGraph::ResourceKey();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        /* Information about the resource. */
        if (currentResource)
        {
            const char* typeStr;
            if (currentResource->type == kRenderResourceType_Texture)
            {
                switch (currentResource->texture.type)
                {
                    case kGPUResourceType_Texture1D: typeStr = "Texture1D"; break;
                    case kGPUResourceType_Texture2D: typeStr = "Texture2D"; break;
                    case kGPUResourceType_Texture3D: typeStr = "Texture3D"; break;
                    default: break;
                }
            }
            else
            {
                typeStr = "Buffer";
            }

            ImGui::Text("Type:     %s", typeStr);
            ImGui::Text("Imported: %s", (currentResource->imported) ? "Yes" : "No");
            ImGui::Text("Required: %s", (currentResource->required) ? "Yes" : "No");
            ImGui::Text("Usage:   ");

            if (currentResource->usage == kGPUResourceUsage_Standard)
            {
                ImGui::SameLine(); ImGui::Text("Standard");
            }
            else
            {
                auto DoUsage = [&] (const GPUResourceUsage usage, const char* const str)
                {
                    if (currentResource->usage & usage)
                    {
                        ImGui::SameLine(); ImGui::Text("%s", str);
                    }
                };

                DoUsage(kGPUResourceUsage_ShaderRead,   "ShaderRead");
                DoUsage(kGPUResourceUsage_ShaderWrite,  "ShaderWrite");
                DoUsage(kGPUResourceUsage_RenderTarget, "RenderTarget");
                DoUsage(kGPUResourceUsage_DepthStencil, "DepthStencil");
            }

            if (currentResource->type == kRenderResourceType_Texture)
            {
                const RenderTextureDesc& texture = currentResource->texture;

                ImGui::Text("Layers:   %u", texture.arraySize);
                ImGui::Text("Mips:     %u", texture.numMipLevels);
                ImGui::Text("Width:    %u", texture.width);

                if (texture.type >= kGPUResourceType_Texture2D)
                {
                    ImGui::Text("Height:   %u", texture.height);

                    if (texture.type >= kGPUResourceType_Texture3D)
                    {
                        ImGui::Text("Depth:    %u", texture.depth);
                    }
                }
            }
            else
            {
                const RenderBufferDesc& buffer = currentResource->buffer;

                ImGui::Text("Size:     %zu (%.2f KiB)",
                            buffer.size,
                            static_cast<float>(buffer.size) / 1024);
            }

            ImGui::NewLine();

            ImGui::Text("Versions:");
            ImGui::PushID("VersionTree");

            /* Nothing produces initial version. */
            for (size_t version = 1; version < currentResource->producers.size(); version++)
            {
                const RenderGraphPass* const producer = currentResource->producers[version];

                ImGui::TreeNodeEx(producer->mName.c_str(),
                                  kLeafFlags | ImGuiTreeNodeFlags_Bullet,
                                  "%zu: %s",
                                  version,
                                  producer->mName.c_str());

                if (ImGui::IsItemClicked())
                {
                    mJumpToPass = GetPassKey(producer);
                }

                if (currentResource->type == kRenderResourceType_Texture)
                {
                    ImGui::PushID(version);
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    if (SmallButton("Output", 50))
                    {
                        RenderGraph::mDebugOutput = GetResourceKey(currentResource);
                        RenderGraph::mDebugOutput.versionProducer = producer->mName;
                    }
                    ImGui::PopID();
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::End();
}
