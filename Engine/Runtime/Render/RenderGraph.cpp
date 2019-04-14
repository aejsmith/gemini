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
#include "GPU/GPURenderPass.h"
#include "GPU/GPUTexture.h"

/**
 * TODO:
 *  - Could add some helper functions for transfer passes for common cases,
 *    e.g. just copying a texture.
 *  - GPU memory aliasing based on required resource lifetimes.
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

    for (const auto& otherUse : mUsedResources)
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
    GPUSubresourceRange range = { 0, 1, 0, 1 };

    if (mGraph.GetResourceType(inHandle) == kRenderResourceType_Texture)
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
    viewHandle.index = mGraph.mViews.size();

    mGraph.mViews.emplace_back();
    RenderGraph::View& view = mGraph.mViews.back();
    view.resourceIndex      = inHandle.index;
    view.desc               = inDesc;

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
    /* TODO: Validate that view is first version of resource. */
    Assert(inIndex < kMaxRenderPassColourAttachments);

    Attachment& attachment = mColour[inIndex];
    Assert(attachment.view);

    attachment.clearData.colour = inValue;
}

void RenderGraphPass::ClearDepth(const float inValue)
{
    /* TODO: Validate that view is first version of resource. */
    Assert(mDepthStencil.view);

    mDepthStencil.clearData.depth = inValue;
}

void RenderGraphPass::ClearStencil(const uint32_t inValue)
{
    /* TODO: Validate that view is first version of resource. */
    Assert(mDepthStencil.view);

    mDepthStencil.clearData.stencil = inValue;
}

RenderGraph::RenderGraph()
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

RenderGraph::Resource::Resource() :
    texture         (),
    currentVersion  (0),
    imported        (nullptr),
    originalState   (kGPUResourceState_None),
    required        (false)
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
    resource->imported      = inResource;
    resource->originalState = inState;
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

void RenderGraph::DetermineRequiredPasses()
{
    /*
     * This function determines which passes are actually required to produce
     * the final outputs of the graph. The final outputs are all imported
     * resources that are written by a graph pass.
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

        for (const auto& use : pass->mUsedResources)
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
}

void RenderGraph::Execute()
{
    DetermineRequiredPasses();

    //AllocateResources();

    Fatal("TODO");
}

GPUBuffer* RenderGraph::GetBuffer(const RenderGraphContext&  inContext,
                                  const RenderResourceHandle inHandle)
{
    Fatal("TODO");
}

GPUTexture* RenderGraph::GetTexture(const RenderGraphContext&  inContext,
                                    const RenderResourceHandle inHandle)
{
    Fatal("TODO");
}

GPUResourceView* RenderGraph::GetView(const RenderGraphContext& inContext,
                                      const RenderViewHandle    inHandle)
{
    Fatal("TODO");
}
