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

#include "Core/Hash.h"

#include "GPU/GPUDefs.h"

/**
 * Various parts of graphics pipeline state are described in blocks, created
 * from a state descriptor.
 *
 * Only one state object matching a given descriptor can exist, and once a
 * state object has been created then it will never be destroyed. This means
 * a pointer to a state object can uniquely identify that state within a run
 * of the engine. This is particularly helpful for pipeline state caching: it
 * means we don't need to hash the whole state descriptor to look up a
 * pipeline.
 *
 * GPUState objects are independent of the GPUDevice. They only contain a state
 * description, which gets consumed by the backend when creating a pipeline.
 */
template <typename D>
class GPUState : Uncopyable
{
public:
    using Desc                = D;

public:
    const Desc&                 GetDesc() const     { return mDesc; }

    /** Get a state ID representing the given descriptor. */
    static const GPUState*      Get(const Desc& inDesc);

private:
                                GPUState(const Desc& inDesc) :
                                    mDesc (inDesc)
                                {}

private:
    struct Cache;

private:
    Desc                        mDesc;

    static Cache                mCache;

};

/** Configuration of colour blending state for the graphics pipeline. */
struct GPUBlendStateDesc
{
    struct Attachment
    {
        bool                    enable : 1;

        /** Whether to disable writing to individual components. */
        bool                    maskR : 1;
        bool                    maskG : 1;
        bool                    maskB : 1;
        bool                    maskA : 1;

        /**
         * Note that for blend factors involving a constant value, the constant
         * is set dynamically using GPUGraphicsCommandList::SetBlendConstants().
         */
        GPUBlendFactor          srcColourFactor;
        GPUBlendFactor          dstColourFactor;
        GPUBlendOp              colourOp;

        GPUBlendFactor          srcAlphaFactor;
        GPUBlendFactor          dstAlphaFactor;
        GPUBlendOp              alphaOp;
    };

    /**
     * Blending state for each colour attachment. If an attachment is disabled
     * in the render target state, the blend state is ignored.
     */
    Attachment                  attachments[kMaxRenderPassColourAttachments];

public:
                                GPUBlendStateDesc();
                                GPUBlendStateDesc(const GPUBlendStateDesc& inOther);

};

DEFINE_HASH_MEM_OPS(GPUBlendStateDesc);

inline GPUBlendStateDesc::GPUBlendStateDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPUBlendStateDesc::GPUBlendStateDesc(const GPUBlendStateDesc& inOther)
{
    memcpy(this, &inOther, sizeof(*this));
}

using GPUBlendState    = GPUState<GPUBlendStateDesc>;
using GPUBlendStateRef = const GPUBlendState*;

/** Configuration of depth/stencil testing state for the graphics pipeline. */
struct GPUDepthStencilStateDesc
{
    struct StencilFace
    {
        GPUStencilOp            failOp;
        GPUStencilOp            passOp;
        GPUStencilOp            depthFailOp;
        GPUCompareOp            compareOp;

        // TODO: Should these be implemented as dynamic state?
        uint32_t                compareMask;
        uint32_t                writeMask;
        uint32_t                reference;
    };

    bool                        depthTestEnable : 1;
    bool                        depthWriteEnable : 1;
    bool                        stencilTestEnable : 1;

    /**
     * Whether to enable the depth bounds test. Bounds are set dynamically
     * using GPUGraphicsCommandList::SetDepthBounds().
     */
    bool                        depthBoundsTestEnable : 1;

    GPUCompareOp                depthCompareOp;

    StencilFace                 stencilFront;
    StencilFace                 stencilBack;

public:
                                GPUDepthStencilStateDesc();
                                GPUDepthStencilStateDesc(const GPUDepthStencilStateDesc& inOther);

};

DEFINE_HASH_MEM_OPS(GPUDepthStencilStateDesc);

inline GPUDepthStencilStateDesc::GPUDepthStencilStateDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPUDepthStencilStateDesc::GPUDepthStencilStateDesc(const GPUDepthStencilStateDesc& inOther)
{
    memcpy(this, &inOther, sizeof(*this));
}

using GPUDepthStencilState    = GPUState<GPUDepthStencilStateDesc>;
using GPUDepthStencilStateRef = const GPUDepthStencilState*;

/** Configuration of rasterizer state for the graphics pipeline. */
struct GPURasterizerStateDesc
{
    GPUPolygonMode              polygonMode;
    GPUCullMode                 cullMode;
    GPUFrontFace                frontFace;
    bool                        depthClampEnable : 1;

    /**
     * Whether to enable depth biasing. Factors are set dynamically using
     * GPUGraphicsCommandList::SetDepthBias().
     */
    bool                        depthBiasEnable : 1;

public:
                                GPURasterizerStateDesc();
                                GPURasterizerStateDesc(const GPURasterizerStateDesc& inOther);

};

DEFINE_HASH_MEM_OPS(GPURasterizerStateDesc);

inline GPURasterizerStateDesc::GPURasterizerStateDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPURasterizerStateDesc::GPURasterizerStateDesc(const GPURasterizerStateDesc& inOther)
{
    memcpy(this, &inOther, sizeof(*this));
}

using GPURasterizerState    = GPUState<GPURasterizerStateDesc>;
using GPURasterizerStateRef = const GPURasterizerState*;

/** Configuration of render target formats for the graphics pipeline. */
struct GPURenderTargetStateDesc
{
    /**
     * Format of colour attachments. Unused attachments are indicated by
     * PixelFormat::kUnknown.
     */
    PixelFormat                 colour[kMaxRenderPassColourAttachments];

    /**
     * Format of the depth/stencil attachment. No depth/stencil is indicated by
     * PixelFormat::kUnknown.
     */
    PixelFormat                 depthStencil;

public:
                                GPURenderTargetStateDesc();
                                GPURenderTargetStateDesc(const GPURenderTargetStateDesc& inOther);

};

DEFINE_HASH_MEM_OPS(GPURenderTargetStateDesc);

inline GPURenderTargetStateDesc::GPURenderTargetStateDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPURenderTargetStateDesc::GPURenderTargetStateDesc(const GPURenderTargetStateDesc& inOther)
{
    memcpy(this, &inOther, sizeof(*this));
}

using GPURenderTargetState    = GPUState<GPURenderTargetStateDesc>;
using GPURenderTargetStateRef = const GPURenderTargetState*;

/** Configuration of vertex inputs for the graphics pipeline. */
struct GPUVertexInputStateDesc
{
    struct Attribute
    {
        /**
         * Format of the attribute. kGPUAttributeFormat_Unknown indicates that
         * this attribute is unused.
         */
        GPUAttributeFormat      format;

        /** Buffer index that this attribute sources data from. */
        uint8_t                 buffer;

        /** Offset from the start of each vertex of the attribute data. */
        uint16_t                offset;
    };

    struct Buffer
    {
        /** Stride between vertices in the buffer. */
        uint16_t                stride;

        /** If true, buffer advances per-instance rather than per-vertex. */
        bool                    perInstance;
    };

    /**
     * Array of attributes. Index into the array matches the location index
     * that the attribute will be available at in shaders.
     */
    Attribute                   attributes[kMaxVertexAttributes];

    /**
     * Array of buffers, referenced by attributes. Only entries referenced by
     * an attribute are paid attention to.
     */
    Buffer                      buffers[kMaxVertexAttributes];

public:
                                GPUVertexInputStateDesc();
                                GPUVertexInputStateDesc(const GPUVertexInputStateDesc& inOther);
};

DEFINE_HASH_MEM_OPS(GPUVertexInputStateDesc);

inline GPUVertexInputStateDesc::GPUVertexInputStateDesc()
{
    /* Ensure that padding is cleared so we can hash the whole structure. */
    memset(this, 0, sizeof(*this));
}

inline GPUVertexInputStateDesc::GPUVertexInputStateDesc(const GPUVertexInputStateDesc& inOther)
{
    memcpy(this, &inOther, sizeof(*this));
}

using GPUVertexInputState    = GPUState<GPUVertexInputStateDesc>;
using GPUVertexInputStateRef = const GPUVertexInputState*;
