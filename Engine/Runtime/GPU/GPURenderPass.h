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

#include "GPU/GPUResourceView.h"

/**
 * Structure defining a render pass. Defines the set of attachments that will
 * be used within the pass, and how they should be used. For colour targets,
 * the index used here corresponds to the colour output index in shaders.
 *
 * When using multiple attachments in a pass, the dimensions (width, height,
 * and layer count) must match between all of them.
 *
 * Note that setting a resource view on this structure does not cause the
 * reference count of the view to be incremented (a non-reference-counting
 * pointer is used). However, passing it to CreateRenderPass() will increment
 * the reference counts, and they will be released when the pass is submitted
 * with SubmitRenderPass().
 */
struct GPURenderPass
{
    struct Attachment
    {
        /**
         * Resource view to use, or null for an unused attachment. For colour
         * attachments, must have kGPUResourceUsage_RenderTarget usage. For
         * depth/stencil, must have kGPUResourceUsage_DepthStencil.
         */
        GPUResourceView*        view;

        /**
         * Resource state that the view will be in at the time where the pass
         * is submitted. For colour attachments, must be
         * kGPUResourceState_RenderTarget.
         *
         * For depth/stencil, can be any one of the depth states, specifying
         * which aspects, if any, will be written by the pass. For aspects that
         * are read-only, the load op must be kGPULoadOp_Load and the store op
         * must be kGPUStoreOp_Store. Other states (e.g. shader read) that a
         * read-only state is paired with should not be specified here.
         */
        GPUResourceState        state;

        /**
         * How to load. For colour attachments, only loadOp. For depth/stencil,
         * loadOp applies to depth and stencilLoadOp applies to stencil.
         */
        GPULoadOp               loadOp;
        GPULoadOp               stencilLoadOp;

        /** How to store. Same as for (stencil)loadOp. */
        GPUStoreOp              storeOp;
        GPUStoreOp              stencilStoreOp;

        /**
         * If either load op is kGPULoadOp_Clear, provides the clear value to
         * use.
         */
        GPUTextureClearData     clearValue;
    };

    Attachment                  colour[kMaxRenderPassColourAttachments];
    Attachment                  depthStencil;

public:
                                GPURenderPass();

public:
    /**
     * Set an attachment. Defaults to kGPULoadOp_Load and kGPUStoreOp_Store.
     * Use the Clear/Discard methods to override this.
     */
    void                        SetColour(const uint8_t          inIndex,
                                          GPUResourceView* const inView);
    void                        SetDepthStencil(GPUResourceView* const inView,
                                                const GPUResourceState inState = kGPUResourceState_DepthStencilWrite);

    void                        ClearColour(const uint8_t    inIndex,
                                            const glm::vec4& inValue);
    void                        ClearDepth(const float inValue);
    void                        ClearStencil(const uint32_t inValue);

    void                        DiscardColour(const uint8_t inIndex);
    void                        DiscardDepth();
    void                        DiscardStencil();

    void                        GetDimensions(uint32_t& outWidth,
                                              uint32_t& outHeight,
                                              uint32_t& outLayers) const;

    #if ORION_BUILD_DEBUG

    void                        Validate() const;

    #endif
};

inline GPURenderPass::GPURenderPass()
{
    memset(this, 0, sizeof(*this));
}

inline void GPURenderPass::SetColour(const uint8_t          inIndex,
                                     GPUResourceView* const inView)
{
    auto& attachment = this->colour[inIndex];

    attachment.view    = inView;
    attachment.state   = kGPUResourceState_RenderTarget;
    attachment.loadOp  = kGPULoadOp_Load;
    attachment.storeOp = kGPUStoreOp_Store;
}

inline void GPURenderPass::SetDepthStencil(GPUResourceView* const inView,
                                           const GPUResourceState inState)
{
    auto& attachment = this->depthStencil;

    attachment.view           = inView;
    attachment.state          = inState;
    attachment.loadOp         = kGPULoadOp_Load;
    attachment.stencilLoadOp  = kGPULoadOp_Load;
    attachment.storeOp        = kGPUStoreOp_Store;
    attachment.stencilStoreOp = kGPUStoreOp_Store;
}

inline void GPURenderPass::ClearColour(const uint8_t    inIndex,
                                       const glm::vec4& inValue)
{
    auto& attachment = this->colour[inIndex];

    attachment.loadOp            = kGPULoadOp_Clear;
    attachment.clearValue.type   = GPUTextureClearData::kColour;
    attachment.clearValue.colour = inValue;
}

inline void GPURenderPass::ClearDepth(const float inValue)
{
    auto& attachment = this->depthStencil;

    attachment.loadOp           = kGPULoadOp_Clear;
    attachment.clearValue.type  = (attachment.stencilLoadOp == kGPULoadOp_Clear)
                                      ? GPUTextureClearData::kDepthStencil
                                      : GPUTextureClearData::kDepth;
    attachment.clearValue.depth = inValue;
}

inline void GPURenderPass::ClearStencil(const uint32_t inValue)
{
    auto& attachment = this->depthStencil;

    attachment.stencilLoadOp      = kGPULoadOp_Clear;
    attachment.clearValue.type    = (attachment.loadOp == kGPULoadOp_Clear)
                                        ? GPUTextureClearData::kDepthStencil
                                        : GPUTextureClearData::kStencil;
    attachment.clearValue.stencil = inValue;
}

inline void GPURenderPass::DiscardColour(const uint8_t inIndex)
{
    this->colour[inIndex].storeOp = kGPUStoreOp_Discard;
}

inline void GPURenderPass::DiscardDepth()
{
    this->depthStencil.storeOp = kGPUStoreOp_Discard;
}

inline void GPURenderPass::DiscardStencil()
{
    this->depthStencil.stencilStoreOp = kGPUStoreOp_Discard;
}
