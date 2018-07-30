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

#include "Core/Math.h"
#include "Core/PixelFormat.h"
#include "Core/Utility.h"

class GPUResource;

/** Maximum number of colour attachments in a render pass. */
static constexpr size_t kMaxRenderPassColourAttachments = 8;

enum GPUResourceUsage : uint32_t
{
    /** Resource will be bound as a read-only shader resource. */
    kGPUResourceUsage_ShaderRead    = (1 << 0),

    /** Resource will be bound as a writable shader resource. */
    kGPUResourceUsage_ShaderWrite   = (1 << 1),

    /** Resource will be bound as a render target. */
    kGPUResourceUsage_RenderTarget  = (1 << 2),

    /** Resource will be bound as a depth/stencil target. */
    kGPUResourceUsage_DepthStencil  = (1 << 3),
};

DEFINE_ENUM_BITWISE_OPS(GPUResourceUsage);

enum GPUResourceType
{
    kGPUResourceType_Buffer,
    kGPUResourceType_Texture1D,
    kGPUResourceType_Texture2D,
    kGPUResourceType_Texture3D,
};

enum GPUTextureFlags : uint32_t
{
    kGPUTexture_None = 0,

    /**
     * Cube resource views can be created of the texture. When used, the type
     * must be kGPUResourceType_2D, and the array size must be specified as a
     * multiple of 6.
     */
    kGPUTexture_CubeCompatible  = (1 << 0),
};

DEFINE_ENUM_BITWISE_OPS(GPUTextureFlags);

enum GPUResourceViewType
{
    /** Untyped view of a buffer. */
    kGPUResourceViewType_Buffer,

    /** Typed view of a buffer. */
    kGPUResourceViewType_TextureBuffer,

    /** 1D texture (array). Texture must be kGPUResourceType_1D. */
    kGPUResourceViewType_Texture1D,
    kGPUResourceViewType_Texture1DArray,

    /** 2D texture (array). Texture must be kGPUResourceType_2D. */
    kGPUResourceViewType_Texture2D,
    kGPUResourceViewType_Texture2DArray,

    /** Cube texture (array). Texture must have kGPUTexture_CubeCompatible. */
    kGPUResourceViewType_TextureCube,
    kGPUResourceViewType_TextureCubeArray,
    kGPUResourceViewType_Texture3D,
};

struct GPUSubresourceRange
{
    uint32_t                    mipOffset;
    uint32_t                    mipCount;
    uint32_t                    layerOffset;
    uint32_t                    layerCount;
};

/**
 * States for a resource. A resource must be in an appropriate state for how it
 * is going to be used at any given point, and resource barriers must be used
 * to transition between states. Each subresource of a resource has its own
 * state - only the subresources included in an access (e.g. those covered by
 * the view used) need to be in the appropriate state for that access.
 *
 * This is essentially a simplification of the various pipeline stage and
 * access type flags in Vulkan into the combinations that actually make sense.
 *
 * Resources can be in multiple states at once, by combining states with
 * bitwise operations, to allow multiple usages (though some states are
 * mutually exclusive, as noted below).
 *
 * A barrier can specify the same state both in the before and after state.
 * This has the effect of ensuring ordering between commands before and after
 * the barrier, e.g. in some instances it is necessary for one draw/dispatch
 * writing to a resource to complete before a following one can write the same
 * resource.
 *
 * At the GPU API level, no attempt is made to automatically manage or track
 * the states of resources. This is up to higher level parts of the engine,
 * such as the frame graph system and high level resource asset classes.
 */
enum GPUResourceState : uint32_t
{
    /**
     * Generic shader read states for each stage. For buffers, this should be
     * used for anything other than uniform read access, which has its own
     * state. For textures, these must be used for access through views
     * that do not have kGPUResourceUsage_ShaderWrite usage.
     */
    kGPUResourceState_VertexShaderRead          = (1 << 0),
    kGPUResourceState_FragmentShaderRead        = (1 << 1),
    kGPUResourceState_ComputeShaderRead         = (1 << 2),

    kGPUResourceState_AllShaderRead             = kGPUResourceState_VertexShaderRead |
                                                  kGPUResourceState_FragmentShaderRead |
                                                  kGPUResourceState_ComputeShaderRead,

    /**
     * Generic shader write states for each stage. Note these also grant read
     * access. These are each mutually exclusive. For textures, these must be
     * used for access through views with kGPUResourceUsage_ShaderWrite usage.
     */
    kGPUResourceState_VertexShaderWrite         = (1 << 3),
    kGPUResourceState_FragmentShaderWrite       = (1 << 4),
    kGPUResourceState_ComputeShaderWrite        = (1 << 5),

    kGPUResourceState_AllShaderWrite            = kGPUResourceState_VertexShaderWrite |
                                                  kGPUResourceState_FragmentShaderWrite |
                                                  kGPUResourceState_ComputeShaderWrite,

    /**
     * Constant buffer read states.
     */
    kGPUResourceState_VertexShaderUniformRead   = (1 << 6),
    kGPUResourceState_FragmentShaderUniformRead = (1 << 7),
    kGPUResourceState_ComputeShaderUniformRead  = (1 << 8),

    kGPUResourceState_AllShaderUniformRead      = kGPUResourceState_VertexShaderUniformRead |
                                                  kGPUResourceState_FragmentShaderUniformRead |
                                                  kGPUResourceState_ComputeShaderUniformRead,

    /**
     * Buffer read states in other parts of the pipeline.
     */
    kGPUResourceState_IndirectBufferRead        = (1 << 9),
    kGPUResourceState_VertexBufferRead          = (1 << 10),
    kGPUResourceState_IndexBufferRead           = (1 << 11),

    /**
     * Render target output. A texture must be in this state to be set as the
     * colour output of a render pass. It can only be applied to textures with
     * kGPUResourceUsage_RenderTarget. This is mutually exclusive.
     */
    kGPUResourceState_RenderTarget              = (1 << 12),

    /**
     * Depth/stencil read/write states. A texture must be in one of these
     * states to be set as the depth/stencil output of a render pass. It can
     * only be applied to textures with kGPUResourceUsage_DepthStencil.
     *
     * kGPUResourceState_DepthStencilWrite is mutually exclusive. The others
     * can be combined with shader read states, which makes it possible to read
     * from the portion of the texture which is indicated read-only in these
     * states. For example, kGPUResourceState_DepthReadStencilWrite can be
     * combined with kGPUResourceState_FragmentShaderRead to allow reading the
     * depth portion of the texture while it is bound in the current render
     * pass.
     */
    kGPUResourceState_DepthStencilWrite         = (1 << 13),
    kGPUResourceState_DepthReadStencilWrite     = (1 << 14),
    kGPUResourceState_DepthWriteStencilRead     = (1 << 15),
    kGPUResourceState_DepthStencilRead          = (1 << 16),

    /**
     * Transfer states. These are each mutually exclusive. Used for any
     * transfer operations, e.g. copies, clears outside render passes.
     */
    kGPUResourceState_TransferRead              = (1 << 17),
    kGPUResourceState_TransferWrite             = (1 << 18),

    /**
     * State for presentation. This is mutually exclusive. This can only be
     * applied to swapchain textures. See GPUComputeContext::{Begin,End}Present()
     * for more details.
     */
    kGPUResourceState_Present                   = (1 << 19),
};

DEFINE_ENUM_BITWISE_OPS(GPUResourceState);

/**
 * Structure describing a resource barrier, for transitioning between different
 * resource states.
 */
struct GPUResourceBarrier
{
    /** Resource to transition. */
    GPUResource*                resource;

    /**
     * Subresource range to transition. If either count is 0, will transition
     * the whole resource.
     */
    GPUSubresourceRange         range;

    /** Current state, and the state to transition to. */
    GPUResourceState            currentState;
    GPUResourceState            newState;

    // TODO: Ownership transfer.
    // TODO: Allow discarding texture content rather than preserving it (e.g.
    // UNDEFINED as previous Vulkan layout).
};

/** Data for clearing a texture. */
struct GPUTextureClearData
{
    /** Type of the clear. This selects the aspect(s) of the texture to clear. */
    enum Type
    {
        kColour,
        kDepth,
        kStencil,
        kDepthStencil,
    };

    Type                        type;

    glm::vec4                   colour;
    float                       depth;
    uint32_t                    stencil;
};

/** How to load the contents of an attachment at the start of a render pass. */
enum GPULoadOp : uint8_t
{
    /** Preserve the existing content of the attachment. */
    kGPULoadOp_Load,

    /** Clear the attachment to the value specified in the render pass. */
    kGPULoadOp_Clear,
};

/** How to store the contents of an attachment at the end of a render pass. */
enum GPUStoreOp : uint8_t
{
    /** Save the new content of the attachment to memory. */
    kGPUStoreOp_Store,

    /**
     * Discard the output. This should be used e.g. for depth attachments which
     * are only used for depth testing within the render pass, and the content
     * is never needed again outside the pass.
     */
    kGPUStoreOp_Discard,
};
