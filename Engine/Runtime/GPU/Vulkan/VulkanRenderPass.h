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

#include "VulkanDefs.h"

#include "VulkanResourceView.h"

#include "Core/Hash.h"
#include "Core/PixelFormat.h"
#include "Core/Utility.h"

#include "GPU/GPURenderPass.h"

struct VulkanRenderPassKey
{
    struct Attachment
    {
        /** Attachment format. kUnknown indicates an unused attachment. */
        PixelFormat             format;

        GPUResourceState        state;

        GPULoadOp               loadOp;
        GPULoadOp               stencilLoadOp;
        GPUStoreOp              storeOp;
        GPUStoreOp              stencilStoreOp;
    };

    Attachment                  colour[kMaxRenderPassColourAttachments];
    Attachment                  depthStencil;

public:
                                VulkanRenderPassKey(const GPURenderPass& pass);
                                VulkanRenderPassKey(const GPURenderTargetStateDesc& state);

};

DEFINE_HASH_MEM_OPS(VulkanRenderPassKey);

inline VulkanRenderPassKey::VulkanRenderPassKey(const GPURenderPass& pass)
{
    /* Ensure padding is clear. */
    memset(this, 0, sizeof(*this));

    for (size_t i = 0; i < ArraySize(this->colour); i++)
    {
        const auto& srcAttachment = pass.colour[i];
        auto& dstAttachment       = this->colour[i];

        if (srcAttachment.view)
        {
            dstAttachment.format  = srcAttachment.view->GetFormat();
            dstAttachment.state   = srcAttachment.state;
            dstAttachment.loadOp  = srcAttachment.loadOp;
            dstAttachment.storeOp = srcAttachment.storeOp;
        }
        else
        {
            dstAttachment.format = kPixelFormat_Unknown;
        }
    }

    const auto& srcAttachment = pass.depthStencil;
    auto& dstAttachment       = this->depthStencil;

    if (srcAttachment.view)
    {
        dstAttachment.format  = srcAttachment.view->GetFormat();
        dstAttachment.state   = srcAttachment.state;
        dstAttachment.loadOp  = srcAttachment.loadOp;
        dstAttachment.storeOp = srcAttachment.storeOp;

        if (PixelFormatInfo::IsDepthStencil(dstAttachment.format))
        {
            dstAttachment.stencilLoadOp  = srcAttachment.stencilLoadOp;
            dstAttachment.stencilStoreOp = srcAttachment.stencilStoreOp;
        }
    }
    else
    {
        dstAttachment.format = kPixelFormat_Unknown;
    }
}

inline VulkanRenderPassKey::VulkanRenderPassKey(const GPURenderTargetStateDesc& state)
{
    /* Ensure padding is clear. */
    memset(this, 0, sizeof(*this));

    for (size_t i = 0; i < ArraySize(this->colour); i++)
    {
        auto& dstAttachment = this->colour[i];

        dstAttachment.format = state.colour[i];

        if (dstAttachment.format != kPixelFormat_Unknown)
        {
            /* Fill out other parts with sensible defaults. There's a reasonable
             * chance this render pass might actually be used for real, rather
             * than just for pipeline creation. */
            dstAttachment.state   = kGPUResourceState_RenderTarget;
            dstAttachment.loadOp  = kGPULoadOp_Load;
            dstAttachment.storeOp = kGPUStoreOp_Store;
        }
    }

    auto& dstAttachment = this->depthStencil;

    dstAttachment.format = state.depthStencil;

    if (dstAttachment.format != kPixelFormat_Unknown)
    {
        dstAttachment.state   = kGPUResourceState_DepthStencilWrite;
        dstAttachment.loadOp  = kGPULoadOp_Load;
        dstAttachment.storeOp = kGPUStoreOp_Store;

        if (PixelFormatInfo::IsDepthStencil(dstAttachment.format))
        {
            dstAttachment.stencilLoadOp  = kGPULoadOp_Load;
            dstAttachment.stencilStoreOp = kGPUStoreOp_Store;
        }
    }
}

struct VulkanFramebufferKey
{
    VkImageView                 colour[kMaxRenderPassColourAttachments];
    VkImageView                 depthStencil;

public:
                                VulkanFramebufferKey(const GPURenderPass& pass);

};

DEFINE_HASH_MEM_OPS(VulkanFramebufferKey);

inline VulkanFramebufferKey::VulkanFramebufferKey(const GPURenderPass& pass)
{
    /* Ensure padding is clear. */
    memset(this, 0, sizeof(*this));

    for (size_t i = 0; i < ArraySize(this->colour); i++)
    {
        const auto view = static_cast<VulkanResourceView*>(pass.colour[i].view);

        if (view)
        {
            this->colour[i] = view->GetImageView();
        }
    }

    const auto view = static_cast<VulkanResourceView*>(pass.depthStencil.view);

    if (view)
    {
        this->depthStencil = view->GetImageView();
    }
}
