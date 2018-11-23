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

#include <vector>

class GPUResource;

/** Maximum number of colour attachments in a render pass. */
static constexpr size_t kMaxRenderPassColourAttachments = 8;

/** Maximum number of shader argument sets. */
static constexpr size_t kMaxArgumentSets = 4;

/** Maximum number of arguments per argument set. */
static constexpr size_t kMaxArgumentsPerSet = 32;

/** Handle to uniform data written within the current frame (see GPUUniformPool). */
using GPUUniforms = uint32_t;
static constexpr GPUUniforms kGPUUniforms_Invalid = std::numeric_limits<uint32_t>::max();

enum GPUVendor : uint8_t
{
    kGPUVendor_Unknown,
    kGPUVendor_AMD,
    kGPUVendor_Intel,
    kGPUVendor_NVIDIA,
};

enum GPUResourceUsage : uint32_t
{
    /** Resource will be bound as a read-only shader resource. */
    kGPUResourceUsage_ShaderRead    = (1 << 0),

    /** Resource will be bound as a writable shader resource. */
    kGPUResourceUsage_ShaderWrite   = (1 << 1),

    /**
     * Texture-only usages.
     */

    /** Resource will be bound as a render target. */
    kGPUResourceUsage_RenderTarget  = (1 << 2),

    /** Resource will be bound as a depth/stencil target. */
    kGPUResourceUsage_DepthStencil  = (1 << 3),
};

DEFINE_ENUM_BITWISE_OPS(GPUResourceUsage);

enum GPUResourceType : uint8_t
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

enum GPUResourceViewType : uint8_t
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
     * Uniform buffer read states.
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

enum GPUShaderStage : uint8_t
{
    kGPUShaderStage_Vertex,
    kGPUShaderStage_Fragment,

    kGPUShaderStage_Compute,

    /** Number of graphics shader stages. They are numbered from 0. */
    kGPUShaderStage_NumGraphics = kGPUShaderStage_Fragment + 1,
};

/** Array containing SPIR-V shader code. */
using GPUShaderCode = std::vector<uint32_t>;

enum GPUBlendFactor : uint8_t
{
    kGPUBlendFactor_Zero,
    kGPUBlendFactor_One,
    kGPUBlendFactor_SrcColour,
    kGPUBlendFactor_OneMinusSrcColour,
    kGPUBlendFactor_DstColour,
    kGPUBlendFactor_OneMinusDstColour,
    kGPUBlendFactor_SrcAlpha,
    kGPUBlendFactor_OneMinusSrcAlpha,
    kGPUBlendFactor_DstAlpha,
    kGPUBlendFactor_OneMinusDstAlpha,
    kGPUBlendFactor_ConstantColour,
    kGPUBlendFactor_OneMinusConstantColour,
    kGPUBlendFactor_ConstantAlpha,
    kGPUBlendFactor_OneMinusConstantAlpha,
    kGPUBlendFactor_SrcAlphaSaturate,
};

enum GPUBlendOp : uint8_t
{
    kGPUBlendOp_Add,
    kGPUBlendOp_Subtract,
    kGPUBlendOp_ReverseSubtract,
    kGPUBlendOp_Min,
    kGPUBlendOp_Max,
};

enum GPUCompareOp : uint8_t
{
    kGPUCompareOp_Never,
    kGPUCompareOp_Less,
    kGPUCompareOp_Equal,
    kGPUCompareOp_LessOrEqual,
    kGPUCompareOp_Greater,
    kGPUCompareOp_NotEqual,
    kGPUCompareOp_GreaterOrEqual,
    kGPUCompareOp_Always,
};

enum GPUStencilOp : uint8_t
{
    kGPUStencilOp_Keep,
    kGPUStencilOp_Zero,
    kGPUStencilOp_Replace,
    kGPUStencilOp_IncrementAndClamp,
    kGPUStencilOp_DecrementAndClamp,
    kGPUStencilOp_Invert,
    kGPUStencilOp_IncrementAndWrap,
    kGPUStencilOp_DecrementAndWrap,
};

enum GPUPolygonMode : uint8_t
{
    kGPUPolygonMode_Fill,
    kGPUPolygonMode_Line,
    kGPUPolygonMode_Point,
};

enum GPUCullMode : uint8_t
{
    kGPUCullMode_Back,
    kGPUCullMode_Front,
    kGPUCullMode_Both,
    kGPUCullMode_None,
};

enum GPUFrontFace : uint8_t
{
    kGPUFrontFace_CounterClockwise,
    kGPUFrontFace_Clockwise,
};

enum GPUPrimitiveTopology : uint8_t
{
    kGPUPrimitiveTopology_PointList,
    kGPUPrimitiveTopology_LineList,
    kGPUPrimitiveTopology_LineStrip,
    kGPUPrimitiveTopology_TriangleList,
    kGPUPrimitiveTopology_TriangleStrip,
    kGPUPrimitiveTopology_TriangleFan,
};

struct GPUViewport
{
    IntRect                     rect;
    float                       minDepth;
    float                       maxDepth;
};

/** Type of an argument to a shader, in an argument set. */
enum GPUArgumentType : uint8_t
{
    /**
     * Uniforms. Uniforms are always rewritten per-frame, and therefore need to
     * be supplied at command recording time regardless of whether using pre-
     * baked or dynamically created argument sets.
     */
    kGPUArgumentType_Uniforms = 0,

    /**
     * Read-only buffer. Buffers used with an argument of this type must have
     * have kGPUResourceUsage_ShaderRead.
     */
    kGPUArgumentType_Buffer = 1,

    /**
     * Read/write buffer. Buffers used with an argument of this type must have
     * have kGPUResourceUsage_ShaderWrite.
     */
    kGPUArgumentType_RWBuffer = 2,

    /**
     * Read-only (sampled) texture. Textures used with an argument of this
     * type must have kGPUResourceUsage_ShaderRead.
     */
    kGPUArgumentType_Texture = 3,

    /**
     * Read/write texture, or an image in GLSL terminology. Textures used with
     * an argument of this type must have kGPUResourceUsage_ShaderWrite.
     */
    kGPUArgumentType_RWTexture = 4,

    /**
     * Read-only texture (typed) buffer. Buffers used with an argument of this
     * type must have have kGPUResourceUsage_ShaderRead.
     */
    kGPUArgumentType_TextureBuffer = 5,

    /**
     * Read/write texture (typed) buffer. Buffers used with an argument of this
     * type must have have kGPUResourceUsage_ShaderWrite.
     */
    kGPUArgumentType_RWTextureBuffer = 6,

    /**
     * Texture sampler.
     */
    kGPUArgumentType_Sampler = 7,

    /* Number of argument types - note some things depend on the order of these. */
    kGPUArgumentTypeCount,
};

enum GPUStagingAccess
{
    /** Staging resource will be used to read back from the GPU. */
    kGPUStagingAccess_Read,

    /** Staging resource will be used to upload data to the GPU. */
    kGPUStagingAccess_Write,
};
