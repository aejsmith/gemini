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

#include "Render/RenderGraph.h"

#include "GPU/GPUBuffer.h"
#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPURenderPass.h"
#include "GPU/GPUStagingResource.h"
#include "GPU/GPUTexture.h"
#include "GPU/GPUUtils.h"

#include "Render/RenderManager.h"

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

RenderGraphPass::RenderGraphPass(RenderGraph&              inGraph,
                                 std::string               inName,
                                 const RenderGraphPassType inType) :
    mGraph      (inGraph),
    mName       (inName),
    mType       (inType),
    mRequired   (false)
{
}

static GPUResourceUsage ResourceUsageFromState(const GPUResourceState inState)
{
    GPUResourceUsage usage = kGPUResourceUsage_Standard;

    if (inState & kGPUResourceState_AllShaderRead)
    {
        usage |= kGPUResourceUsage_ShaderRead;
    }

    if (inState & kGPUResourceState_AllShaderWrite)
    {
        usage |= kGPUResourceUsage_ShaderWrite;
    }

    if (inState & kGPUResourceState_RenderTarget)
    {
        usage |= kGPUResourceUsage_RenderTarget;
    }

    if (inState & kGPUResourceState_AllDepthStencil)
    {
        usage |= kGPUResourceUsage_DepthStencil;
    }

    Assert(usage == kGPUResourceUsage_Standard || IsOnlyOneBitSet(usage));

    return usage;
}

void RenderGraphPass::UseResource(const RenderResourceHandle inHandle,
                                  const GPUSubresourceRange& inRange,
                                  const GPUResourceState     inState,
                                  RenderResourceHandle*      outNewHandle)
{
    RenderGraph::Resource* resource = mGraph.mResources[inHandle.index];

    const bool isWrite = inState & kGPUResourceState_AllWrite;

    AssertMsg(isWrite || !outNewHandle,
              "outNewHandle must be null for a read-only access");
    AssertMsg(resource->currentVersion == inHandle.version,
              "Resource access must be to current version (see TODO)");

    GPUUtils::ValidateResourceState(inState, resource->type == kRenderResourceType_Texture);

    for (const RenderGraphPass::UsedResource& otherUse : mUsedResources)
    {
        if (otherUse.handle.index == inHandle.index)
        {
            AssertMsg(!otherUse.range.Overlaps(inRange),
                      "Subresources cannot be used multiple times in the same pass");
        }
    }

    mUsedResources.emplace_back();
    UsedResource& use = mUsedResources.back();
    use.handle        = inHandle;
    use.range         = inRange;
    use.state         = inState;

    /* Add required usage flags for this resource state. */
    resource->usage |= ResourceUsageFromState(inState);

    if (isWrite)
    {
        resource->currentVersion++;

        Assert(resource->producers.size() == resource->currentVersion);

        resource->producers.emplace_back(this);

        if (outNewHandle)
        {
            outNewHandle->index   = inHandle.index;
            outNewHandle->version = resource->currentVersion;
        }
    }
}

RenderViewHandle RenderGraphPass::CreateView(const RenderResourceHandle inHandle,
                                             const RenderViewDesc&      inDesc,
                                             RenderResourceHandle*      outNewHandle)
{
    const bool isTexture = mGraph.GetResourceType(inHandle) == kRenderResourceType_Texture;

    GPUSubresourceRange range = { 0, 1, 0, 1 };

    if (isTexture)
    {
        range.mipOffset   = inDesc.mipOffset;
        range.mipCount    = inDesc.mipCount;
        range.layerOffset = inDesc.elementOffset;
        range.layerCount  = inDesc.elementCount;
    }
    else
    {
        Assert(inDesc.mipOffset == 0 && inDesc.mipCount == 1);
    }

    UseResource(inHandle,
                range,
                inDesc.state,
                outNewHandle);

    RenderViewHandle viewHandle;
    viewHandle.pass  = this;
    viewHandle.index = mViews.size();

    mViews.emplace_back();
    View& view = mViews.back();
    view.resource = inHandle;
    view.desc     = inDesc;
    view.view     = nullptr;

    if (isTexture && view.desc.format == kPixelFormat_Unknown)
    {
        /* Set from texture format. */
        view.desc.format = mGraph.GetTextureDesc(inHandle).format;
    }

    return viewHandle;
}

void RenderGraphPass::SetColour(const uint8_t              inIndex,
                                const RenderResourceHandle inHandle,
                                RenderResourceHandle*      outNewHandle)
{
    const RenderTextureDesc& textureDesc = mGraph.GetTextureDesc(inHandle);

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = kGPUResourceState_RenderTarget;
    viewDesc.format = textureDesc.format;

    SetColour(inIndex, inHandle, viewDesc, outNewHandle);
}

void RenderGraphPass::SetColour(const uint8_t              inIndex,
                                const RenderResourceHandle inHandle,
                                const RenderViewDesc&      inDesc,
                                RenderResourceHandle*      outNewHandle)
{
    Assert(mType == kRenderGraphPassType_Render);
    Assert(inIndex < kMaxRenderPassColourAttachments);
    Assert(mGraph.GetResourceType(inHandle) == kRenderResourceType_Texture);
    Assert(inDesc.state == kGPUResourceState_RenderTarget);
    Assert(PixelFormatInfo::IsColour(inDesc.format));
    Assert(outNewHandle);

    Attachment& attachment = mColour[inIndex];
    attachment.view = CreateView(inHandle, inDesc, outNewHandle);

    /* If this is the first version of the resource, it will be cleared, so set
     * a default clear value. */
    attachment.clearData.type   = GPUTextureClearData::kColour;
    attachment.clearData.colour = glm::vec4(0.0f);
}

void RenderGraphPass::SetDepthStencil(const RenderResourceHandle inHandle,
                                      const GPUResourceState     inState,
                                      RenderResourceHandle*      outNewHandle)
{
    const RenderTextureDesc& textureDesc = mGraph.GetTextureDesc(inHandle);

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = inState;
    viewDesc.format = textureDesc.format;

    SetDepthStencil(inHandle, viewDesc, outNewHandle);
}

void RenderGraphPass::SetDepthStencil(const RenderResourceHandle inHandle,
                                      const RenderViewDesc&      inDesc,
                                      RenderResourceHandle*      outNewHandle)
{
    Assert(mType == kRenderGraphPassType_Render);
    Assert(mGraph.GetResourceType(inHandle) == kRenderResourceType_Texture);
    Assert(inDesc.state & kGPUResourceState_AllDepthStencil && IsOnlyOneBitSet(inDesc.state));
    Assert(PixelFormatInfo::IsDepth(inDesc.format));

    mDepthStencil.view = CreateView(inHandle, inDesc, outNewHandle);

    /* If this is the first version of the resource, it will be cleared, so set
     * a default clear value. */
    mDepthStencil.clearData.type    = (PixelFormatInfo::IsDepthStencil(inDesc.format))
                                          ? GPUTextureClearData::kDepthStencil
                                          : GPUTextureClearData::kDepth;
    mDepthStencil.clearData.depth   = 1.0f;
    mDepthStencil.clearData.stencil = 0;
}

void RenderGraphPass::ClearColour(const uint8_t    inIndex,
                                  const glm::vec4& inValue)
{
    Assert(inIndex < kMaxRenderPassColourAttachments);

    Attachment& attachment = mColour[inIndex];
    Assert(attachment.view);

    View& view = mViews[attachment.view.index];
    Assert(view.resource.version == 0);
    Unused(view);

    attachment.clearData.colour = inValue;
}

void RenderGraphPass::ClearDepth(const float inValue)
{
    Assert(mDepthStencil.view);

    View& view = mViews[mDepthStencil.view.index];
    Assert(view.resource.version == 0);
    Unused(view);

    mDepthStencil.clearData.depth = inValue;
}

void RenderGraphPass::ClearStencil(const uint32_t inValue)
{
    Assert(mDepthStencil.view);

    View& view = mViews[mDepthStencil.view.index];
    Assert(view.resource.version == 0);
    Unused(view);

    mDepthStencil.clearData.stencil = inValue;
}

GPUResourceView* RenderGraphPass::GetView(const RenderViewHandle inHandle) const
{
    Assert(inHandle.pass == this);
    Assert(mGraph.mIsExecuting);
    Assert(inHandle.index < mViews.size());
    AssertMsg(mViews[inHandle.index].view, "Attempt to use view of culled resource");

    return mViews[inHandle.index].view;
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

RenderGraphPass& RenderGraph::AddPass(std::string               inName,
                                      const RenderGraphPassType inType)
{
    RenderGraphPass* pass = new RenderGraphPass(*this,
                                                std::move(inName),
                                                inType);

    mPasses.emplace_back(pass);

    return *pass;
}

RenderGraphPass& RenderGraph::AddBlitPass(std::string                inName,
                                          const RenderResourceHandle inDestHandle,
                                          const GPUSubresource       inDestSubresource,
                                          const RenderResourceHandle inSourceHandle,
                                          const GPUSubresource       inSourceSubresource,
                                          RenderResourceHandle*      outNewHandle)
{
    Assert(GetResourceType(inDestHandle) == kRenderResourceType_Texture);
    Assert(GetResourceType(inSourceHandle) == kRenderResourceType_Texture);
    Assert(outNewHandle);

    RenderGraphPass& pass = AddPass(std::move(inName), kRenderGraphPassType_Transfer);

    pass.UseResource(inSourceHandle,
                     inSourceSubresource,
                     kGPUResourceState_TransferRead,
                     nullptr);

    pass.UseResource(inDestHandle,
                     inDestSubresource,
                     kGPUResourceState_TransferWrite,
                     outNewHandle);

    pass.SetFunction([=] (const RenderGraph&     inGraph,
                          const RenderGraphPass& inPass,
                          GPUTransferContext&    inContext)
    {
        inContext.BlitTexture(inGraph.GetTexture(inDestHandle),
                              inDestSubresource,
                              inGraph.GetTexture(inSourceHandle),
                              inSourceSubresource);
    });

    return pass;
}

RenderGraphPass& RenderGraph::AddUploadPass(std::string                inName,
                                            const RenderResourceHandle inDestHandle,
                                            const uint32_t             inDestOffset,
                                            GPUStagingBuffer           inSourceBuffer,
                                            RenderResourceHandle*      outNewHandle)
{
    Assert(GetResourceType(inDestHandle) == kRenderResourceType_Buffer);
    Assert(outNewHandle);

    RenderGraphPass& pass = AddPass(std::move(inName), kRenderGraphPassType_Transfer);

    pass.UseResource(inDestHandle,
                     GPUSubresource{0, 0},
                     kGPUResourceState_TransferWrite,
                     outNewHandle);

    /* Ugh, non-copyable objects can't be captured in a std::function since
     * that is copyable. Make a transient allocation to hold the staging handle
     * instead. */
    auto sourceBuffer = NewTransient<GPUStagingBuffer>(std::move(inSourceBuffer));

    pass.SetFunction([inDestHandle, inDestOffset, sourceBuffer]
                     (const RenderGraph&     inGraph,
                      const RenderGraphPass& inPass,
                      GPUTransferContext&    inContext)
    {
        inContext.UploadBuffer(inGraph.GetBuffer(inDestHandle),
                               *sourceBuffer,
                               sourceBuffer->GetSize(),
                               inDestOffset,
                               0);
    });

    return pass;
}

RenderGraph::Resource::Resource() :
    texture         (),
    usage           (kGPUResourceUsage_Standard),
    currentVersion  (0),
    originalState   (kGPUResourceState_None),
    imported        (false),
    required        (false),
    begun           (false),
    firstPass       (nullptr),
    lastPass        (nullptr),
    resource        (nullptr),
    currentState    (kGPUResourceState_None)
{
    /* Nothing produced the initial version. */
    producers.emplace_back(nullptr);
}

RenderResourceHandle RenderGraph::CreateBuffer(const RenderBufferDesc& inDesc)
{
    Resource* resource = new Resource;
    resource->type     = kRenderResourceType_Buffer;
    resource->buffer   = inDesc;

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = resource->currentVersion;

    mResources.emplace_back(resource);

    return handle;
}

RenderResourceHandle RenderGraph::CreateTexture(const RenderTextureDesc& inDesc)
{
    Resource* resource = new Resource;
    resource->type     = kRenderResourceType_Texture;
    resource->texture  = inDesc;

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = resource->currentVersion;

    mResources.emplace_back(resource);

    return handle;
}

RenderResourceHandle RenderGraph::ImportResource(GPUResource* const     inResource,
                                                 const GPUResourceState inState,
                                                 std::function<void ()> inBeginCallback,
                                                 std::function<void ()> inEndCallback)
{
    Resource* resource      = new Resource;
    resource->imported      = true;
    resource->resource      = inResource;
    resource->originalState = inState;
    resource->currentState  = inState;
    resource->beginCallback = std::move(inBeginCallback);
    resource->endCallback   = std::move(inEndCallback);

    if (inResource->IsTexture())
    {
        resource->type = kRenderResourceType_Texture;

        const auto texture      = static_cast<const GPUTexture*>(inResource);
        RenderTextureDesc& desc = resource->texture;

        desc.name         = nullptr;
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

        const auto buffer      = static_cast<const GPUBuffer*>(inResource);
        RenderBufferDesc& desc = resource->buffer;

        desc.name = nullptr;
        desc.size = buffer->GetSize();
    }

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = resource->currentVersion;

    mResources.emplace_back(resource);

    return handle;
}

void RenderGraph::TransitionResource(Resource&                  inResource,
                                     const GPUSubresourceRange& inRange,
                                     const GPUResourceState     inState)
{
    const bool isWholeResource = inRange == inResource.resource->GetSubresourceRange();

    if (!isWholeResource)
    {
        Fatal("TODO: Per-subresource state tracking");
    }

    if (inResource.currentState != inState)
    {
        mBarriers.emplace_back();
        GPUResourceBarrier& barrier = mBarriers.back();
        barrier.resource     = inResource.resource;
        barrier.range        = inRange;
        barrier.currentState = inResource.currentState;
        barrier.newState     = inState;

        /* Discard if state is currently none, i.e. this is first use. */
        barrier.discard = inResource.currentState == kGPUResourceState_None;

        inResource.currentState = inState;
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

    /* Get the passes producing imported resources. */
    for (const Resource* const resource : mResources)
    {
        if (resource->imported && resource->currentVersion > 0)
        {
            passes[passCount++] = resource->producers[resource->currentVersion];
        }
    }

    while (passCount > 0)
    {
        RenderGraphPass* const pass = passes[--passCount];

        pass->mRequired = true;

        for (const RenderGraphPass::UsedResource& use : pass->mUsedResources)
        {
            Resource* const resource = mResources[use.handle.index];

            resource->required = true;

            RenderGraphPass* const producer = resource->producers[use.handle.version];
            Assert(producer || use.handle.version == 0);

            /* Don't revisit passes we've already been to. */
            if (producer && !producer->mRequired)
            {
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
                desc.usage = resource->usage;
                desc.size  = resource->buffer.size;

                resource->resource = RenderManager::Get().GetTransientBuffer(desc, {});

                break;
            }

            case kRenderResourceType_Texture:
            {
                GPUTextureDesc desc;
                desc.type         = resource->texture.type;
                desc.usage        = resource->usage;
                desc.flags        = resource->texture.flags;
                desc.format       = resource->texture.format;
                desc.width        = resource->texture.width;
                desc.height       = resource->texture.height;
                desc.depth        = resource->texture.depth;
                desc.arraySize    = resource->texture.arraySize;
                desc.numMipLevels = resource->texture.numMipLevels;

                resource->resource = RenderManager::Get().GetTransientTexture(desc, {});

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
    /* Transition imported resources back to the original state. */
    for (Resource* resource : mResources)
    {
        if (resource->begun && resource->imported)
        {
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

void RenderGraph::PrepareResources(RenderGraphPass& inPass)
{
    for (const RenderGraphPass::UsedResource& use : inPass.mUsedResources)
    {
        Resource* const resource = mResources[use.handle.index];

        /* If this is the first pass to use the resource and it has a begin
         * callback, call that now. Could have multiple uses of a resource
         * within the pass, only begin once. */
        if (resource->firstPass == &inPass && !resource->begun)
        {
            if (resource->beginCallback)
            {
                resource->beginCallback();
            }

            resource->begun = true;
        }

        TransitionResource(*resource, use.range, use.state);
    }

    FlushBarriers();
}

void RenderGraph::CreateViews(RenderGraphPass& inPass)
{
    for (RenderGraphPass::View& view : inPass.mViews)
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

void RenderGraph::DestroyViews(RenderGraphPass& inPass)
{
    for (RenderGraphPass::View& view : inPass.mViews)
    {
        delete view.view;
        view.view = nullptr;
    }
}

void RenderGraph::ExecutePass(RenderGraphPass& inPass)
{
    switch (inPass.mType)
    {
        case kRenderGraphPassType_Render:
        {
            Assert(inPass.mRenderFunction);

            GPUGraphicsContext& context = GPUGraphicsContext::Get();

            GPURenderPass renderPass;

            for (size_t i = 0; i < kMaxRenderPassColourAttachments; i++)
            {
                RenderGraphPass::Attachment& colourAtt = inPass.mColour[i];

                if (colourAtt.view)
                {
                    RenderGraphPass::View& view = inPass.mViews[colourAtt.view.index];

                    renderPass.SetColour(i, view.view);

                    /* If this is the first pass to use the resource, clear it.
                     * If it is the last, discard it, unless it is an imported
                     * resource. TODO: Wouldn't always want to clear imported
                     * resources, but do sometimes. */
                    Resource* const resource = mResources[view.resource.index];

                    if (resource->firstPass == &inPass)
                    {
                        renderPass.ClearColour(i, colourAtt.clearData.colour);
                    }

                    if (!resource->imported && resource->lastPass == &inPass)
                    {
                        renderPass.DiscardColour(i);
                    }
                }
            }

            RenderGraphPass::Attachment& depthAtt = inPass.mDepthStencil;

            if (depthAtt.view)
            {
                RenderGraphPass::View& view = inPass.mViews[depthAtt.view.index];

                renderPass.SetDepthStencil(view.view);

                Resource* const resource = mResources[view.resource.index];

                if (resource->firstPass == &inPass)
                {
                    renderPass.ClearDepth(depthAtt.clearData.depth);

                    if (PixelFormatInfo::IsDepthStencil(resource->texture.format))
                    {
                        renderPass.ClearStencil(depthAtt.clearData.stencil);
                    }
                }

                if (!resource->imported && resource->lastPass == &inPass)
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

            inPass.mRenderFunction(*this, inPass, *cmdList);

            cmdList->End();
            context.SubmitRenderPass(cmdList);
            break;
        }

        case kRenderGraphPassType_Compute:
        {
            Assert(inPass.mComputeFunction);

            /* TODO: Async compute. */
            GPUComputeContext& context = GPUGraphicsContext::Get();

            GPUComputeCommandList* const cmdList = context.CreateComputePass();
            cmdList->Begin();

            inPass.mComputeFunction(*this, inPass, *cmdList);

            cmdList->End();
            context.SubmitComputePass(cmdList);
            break;
        }

        case kRenderGraphPassType_Transfer:
        {
            Assert(inPass.mTransferFunction);

            /* Transfer passes are just executed on the main graphics context.
             * Not worth using a transfer queue for mid-frame transfers, it'll
             * just add synchronisation overhead. TODO: Any use case for doing
             * transfers on the async compute queue, i.e. between async compute
             * passes? */
            inPass.mTransferFunction(*this, inPass, GPUGraphicsContext::Get());
            break;
        }

        default:
        {
            Unreachable();
            break;
        }
    }
}

void RenderGraph::Execute()
{
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
}

GPUBuffer* RenderGraph::GetBuffer(const RenderResourceHandle inHandle) const
{
    Assert(mIsExecuting);
    Assert(GetResourceType(inHandle) == kRenderResourceType_Buffer);
    AssertMsg(mResources[inHandle.index]->resource, "Attempt to use culled resource");

    return static_cast<GPUBuffer*>(mResources[inHandle.index]->resource);
}

GPUTexture* RenderGraph::GetTexture(const RenderResourceHandle inHandle) const
{
    Assert(mIsExecuting);
    Assert(GetResourceType(inHandle) == kRenderResourceType_Texture);
    AssertMsg(mResources[inHandle.index]->resource, "Attempt to use culled resource");

    return static_cast<GPUTexture*>(mResources[inHandle.index]->resource);
}

void RenderGraph::AddDestructor(Destructor inDestructor)
{
    mDestructors.emplace_back(std::move(inDestructor));
}
