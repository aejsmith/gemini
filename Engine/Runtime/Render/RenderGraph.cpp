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
 *  - Use FrameAllocator for internal allocations (including STL stuff). Also
 *    could do with a way to get GPU layer objects (resources, views) to be
 *    allocated with it as well.
 */

RenderGraphPass::RenderGraphPass(RenderGraph&              inGraph,
                                 std::string               inName,
                                 const RenderGraphPassType inType) :
    mGraph  (inGraph),
    mName   (inName),
    mType   (inType)
{
}

void RenderGraphPass::UseResource(const RenderResourceHandle inResource,
                                  const GPUResourceState     inState,
                                  const GPUSubresourceRange& inRange,
                                  RenderResourceHandle*      outNewResource)
{
//subresources! think about usage tracking
    Fatal("TODO");
}

RenderViewHandle RenderGraphPass::CreateView(const RenderResourceHandle inResource,
                                             const RenderViewDesc&      inDesc,
                                             RenderResourceHandle*      outNewResource)
{
    Fatal("TODO");
}

void RenderGraphPass::SetColour(const uint8_t              inIndex,
                                const RenderResourceHandle inResource,
                                RenderResourceHandle*      outNewResource)
{
    const RenderTextureDesc& textureDesc = mGraph.GetTextureDesc(inResource);

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = kGPUResourceState_RenderTarget;
    viewDesc.format = textureDesc.format;

    SetColour(inIndex, inResource, viewDesc, outNewResource);
}

void RenderGraphPass::SetColour(const uint8_t              inIndex,
                                const RenderResourceHandle inResource,
                                const RenderViewDesc&      inDesc,
                                RenderResourceHandle*      outNewResource)
{
    Assert(inIndex < kMaxRenderPassColourAttachments);
    Assert(mGraph.GetResourceType(inResource) == kRenderResourceType_Texture);
    Assert(inDesc.state == kGPUResourceState_RenderTarget);
    Assert(PixelFormatInfo::IsColour(inDesc.format));
    Assert(outNewResource);

    Attachment& attachment = mColour[inIndex];
    attachment.view = CreateView(inResource, inDesc, outNewResource);

    /* If this is the first version of the resource, it will be cleared, so set
     * a default clear value. */
    attachment.clearData.type   = GPUTextureClearData::kColour;
    attachment.clearData.colour = glm::vec4(0.0f);
}

void RenderGraphPass::SetDepthStencil(const RenderResourceHandle inResource,
                                      const GPUResourceState     inState,
                                      RenderResourceHandle*      outNewResource)
{
    const RenderTextureDesc& textureDesc = mGraph.GetTextureDesc(inResource);

    RenderViewDesc viewDesc;
    viewDesc.type   = kGPUResourceViewType_Texture2D;
    viewDesc.state  = inState;
    viewDesc.format = textureDesc.format;

    SetDepthStencil(inResource, viewDesc, outNewResource);
}

void RenderGraphPass::SetDepthStencil(const RenderResourceHandle inResource,
                                      const RenderViewDesc&      inDesc,
                                      RenderResourceHandle*      outNewResource)
{
    Assert(mGraph.GetResourceType(inResource) == kRenderResourceType_Texture);
    Assert(inDesc.state & kGPUResourceState_AllDepthStencil && IsOnlyOneBitSet(inDesc.state));
    Assert(PixelFormatInfo::IsDepth(inDesc.format));

    mDepthStencil.view = CreateView(inResource, inDesc, outNewResource);

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

RenderResourceHandle RenderGraph::CreateBuffer(const RenderBufferDesc& inDesc)
{
    Resource* resource      = new Resource;
    resource->type          = kRenderResourceType_Buffer;
    resource->buffer        = inDesc;
    resource->imported      = nullptr;
    resource->originalState = kGPUResourceState_None;

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = 0;

    mResources.emplace_back(resource);

    return handle;
}

RenderResourceHandle RenderGraph::CreateTexture(const RenderTextureDesc& inDesc)
{
    Resource* resource      = new Resource;
    resource->type          = kRenderResourceType_Texture;
    resource->texture       = inDesc;
    resource->imported      = nullptr;
    resource->originalState = kGPUResourceState_None;

    RenderResourceHandle handle;
    handle.index   = mResources.size();
    handle.version = 0;

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
    handle.version = 0;

    mResources.emplace_back(resource);

    return handle;
}

void RenderGraph::Execute()
{
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
