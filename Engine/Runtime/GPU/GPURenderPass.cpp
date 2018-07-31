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

#include "GPU/GPURenderPass.h"

#include "GPU/GPUTexture.h"

void GPURenderPass::GetDimensions(uint32_t& outWidth,
                                  uint32_t& outHeight,
                                  uint32_t& outLayers) const
{
    /* All dimensions should match so just use the first attachment we find. */
    GPUResourceView* view = this->depthStencil.view;
    for (size_t i = 0; !view && i < kMaxRenderPassColourAttachments; i++)
    {
        view = this->colour[i].view;
    }

    Assert(view);

    const auto& texture = static_cast<const GPUTexture&>(view->GetResource());

    outWidth  = texture.GetMipWidth(view->GetMipOffset());
    outHeight = texture.GetMipHeight(view->GetMipOffset());
    outLayers = view->GetElementCount();
}

#if ORION_BUILD_DEBUG

void GPURenderPass::Validate() const
{
    uint32_t width  = std::numeric_limits<uint32_t>::max();
    uint32_t height = std::numeric_limits<uint32_t>::max();
    uint32_t layers = std::numeric_limits<uint32_t>::max();

    auto CheckSize =
        [&] (const GPUResourceView* const inView)
        {
            const auto& texture = static_cast<const GPUTexture&>(inView->GetResource());

            const uint32_t viewWidth  = texture.GetMipWidth(inView->GetMipOffset());
            const uint32_t viewHeight = texture.GetMipHeight(inView->GetMipOffset());
            const uint32_t viewLayers = inView->GetElementCount();

            if (width != std::numeric_limits<uint32_t>::max())
            {
                Assert(viewWidth == width && viewHeight == height && viewLayers == layers);
            }
            else
            {
                width  = viewWidth;
                height = viewHeight;
                layers = viewLayers;
            }
        };

    for (uint32_t i = 0; i < ArraySize(this->colour); i++)
    {
        const auto& attachment = this->colour[i];

        if (attachment.view)
        {
            Assert(attachment.view->GetUsage() == kGPUResourceUsage_RenderTarget);
            Assert(attachment.state == kGPUResourceState_RenderTarget);

            if (attachment.loadOp == kGPULoadOp_Clear)
            {
                Assert(attachment.clearValue.type == GPUTextureClearData::kColour);
            }

            CheckSize(attachment.view);
        }
    }

    const auto& attachment = this->depthStencil;

    if (attachment.view)
    {
        static constexpr GPUResourceState kGPUResourceState_AllDepthStencil =
            kGPUResourceState_DepthStencilWrite |
            kGPUResourceState_DepthReadStencilWrite |
            kGPUResourceState_DepthWriteStencilRead |
            kGPUResourceState_DepthStencilRead;

        Assert(attachment.view->GetUsage() == kGPUResourceUsage_DepthStencil);
        Assert(attachment.state & kGPUResourceState_AllDepthStencil);
        Assert(IsOnlyOneBitSet(attachment.state));

        if (attachment.loadOp == kGPULoadOp_Clear)
        {
            Assert(attachment.clearValue.type == GPUTextureClearData::kDepth || attachment.clearValue.type == GPUTextureClearData::kDepthStencil);
            Assert(attachment.state == kGPUResourceState_DepthStencilWrite || attachment.state == kGPUResourceState_DepthWriteStencilRead);
        }

        if (attachment.stencilLoadOp == kGPULoadOp_Clear)
        {
            Assert(attachment.clearValue.type == GPUTextureClearData::kStencil || attachment.clearValue.type == GPUTextureClearData::kDepthStencil);
            Assert(attachment.state == kGPUResourceState_DepthStencilWrite || attachment.state == kGPUResourceState_DepthReadStencilWrite);
        }

        if (attachment.storeOp == kGPUStoreOp_Discard)
        {
            Assert(attachment.state == kGPUResourceState_DepthStencilWrite || attachment.state == kGPUResourceState_DepthWriteStencilRead);
        }

        if (attachment.stencilStoreOp == kGPUStoreOp_Discard)
        {
            Assert(attachment.state == kGPUResourceState_DepthStencilWrite || attachment.state == kGPUResourceState_DepthReadStencilWrite);
        }

        CheckSize(attachment.view);
    }

    /* TODO: Could allow attachmentless rendering (writing to a ShaderWrite
     * resource), in which case we'd need dimensions specified here somehow. */
    Assert(width != std::numeric_limits<uint32_t>::max());
}

#endif
