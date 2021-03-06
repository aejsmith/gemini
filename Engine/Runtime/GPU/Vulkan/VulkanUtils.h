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

#include "GPU/GPUDefs.h"

namespace VulkanUtils
{
    template <typename T, typename U>
    inline void AddToPNextChain(T& dest, U& add)
    {
        add.pNext  = const_cast<decltype(add.pNext)>(dest.pNext);
        dest.pNext = &add;
    }

    inline VkAttachmentLoadOp ConvertLoadOp(const GPULoadOp op)
    {
        switch (op)
        {
            case kGPULoadOp_Load:   return VK_ATTACHMENT_LOAD_OP_LOAD;
            case kGPULoadOp_Clear:  return VK_ATTACHMENT_LOAD_OP_CLEAR;

            default:
                UnreachableMsg("Unrecognised GPULoadOp");

        }
    }

    inline VkAttachmentStoreOp ConvertStoreOp(const GPUStoreOp op)
    {
        switch (op)
        {
            case kGPUStoreOp_Store:     return VK_ATTACHMENT_STORE_OP_STORE;
            case kGPUStoreOp_Discard:   return VK_ATTACHMENT_STORE_OP_DONT_CARE;

            default:
                UnreachableMsg("Unrecognised GPUStoreOp");

        }
    }

    inline VkShaderStageFlagBits ConvertShaderStage(const GPUShaderStage stage)
    {
        switch (stage)
        {
            case kGPUShaderStage_Vertex:    return VK_SHADER_STAGE_VERTEX_BIT;
            case kGPUShaderStage_Pixel:     return VK_SHADER_STAGE_FRAGMENT_BIT;
            case kGPUShaderStage_Compute:   return VK_SHADER_STAGE_COMPUTE_BIT;

            default:
                UnreachableMsg("Unrecognised GPUShaderStage");

        }
    }

    inline VkPrimitiveTopology ConvertPrimitiveTopology(const GPUPrimitiveTopology topology)
    {
        switch (topology)
        {
            case kGPUPrimitiveTopology_PointList:       return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case kGPUPrimitiveTopology_LineList:        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case kGPUPrimitiveTopology_LineStrip:       return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case kGPUPrimitiveTopology_TriangleList:    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case kGPUPrimitiveTopology_TriangleStrip:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case kGPUPrimitiveTopology_TriangleFan:     return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

            default:
                UnreachableMsg("Unrecognised GPUPrimitiveTopology");

        }
    }

    inline VkPolygonMode ConvertPolygonMode(const GPUPolygonMode polygonMode)
    {
        switch (polygonMode)
        {
            case kGPUPolygonMode_Fill:  return VK_POLYGON_MODE_FILL;
            case kGPUPolygonMode_Line:  return VK_POLYGON_MODE_LINE;
            case kGPUPolygonMode_Point: return VK_POLYGON_MODE_POINT;

            default:
                UnreachableMsg("Unrecognised GPUPolygonMode");

        }
    }

    inline VkCullModeFlags ConvertCullMode(const GPUCullMode cullMode)
    {
        switch (cullMode)
        {
            case kGPUCullMode_Back:     return VK_CULL_MODE_BACK_BIT;
            case kGPUCullMode_Front:    return VK_CULL_MODE_FRONT_BIT;
            case kGPUCullMode_Both:     return VK_CULL_MODE_FRONT_AND_BACK;
            case kGPUCullMode_None:     return VK_CULL_MODE_NONE;

            default:
                UnreachableMsg("Unrecognised GPUCullMode");

        }
    }

    inline VkFrontFace ConvertFrontFace(const GPUFrontFace frontFace)
    {
        switch (frontFace)
        {
            case kGPUFrontFace_CounterClockwise:    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            case kGPUFrontFace_Clockwise:           return VK_FRONT_FACE_CLOCKWISE;

            default:
                UnreachableMsg("Unrecognised GPUFrontFace");

        }
    }

    inline VkCompareOp ConvertCompareOp(const GPUCompareOp compareOp)
    {
        switch (compareOp)
        {
            case kGPUCompareOp_Never:           return VK_COMPARE_OP_NEVER;
            case kGPUCompareOp_Less:            return VK_COMPARE_OP_LESS;
            case kGPUCompareOp_Equal:           return VK_COMPARE_OP_EQUAL;
            case kGPUCompareOp_LessOrEqual:     return VK_COMPARE_OP_LESS_OR_EQUAL;
            case kGPUCompareOp_Greater:         return VK_COMPARE_OP_GREATER;
            case kGPUCompareOp_NotEqual:        return VK_COMPARE_OP_NOT_EQUAL;
            case kGPUCompareOp_GreaterOrEqual:  return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case kGPUCompareOp_Always:          return VK_COMPARE_OP_ALWAYS;

            default:
                UnreachableMsg("Unrecognised GPUCompareOp");
        }
    }

    inline VkStencilOp ConvertStencilOp(const GPUStencilOp stencilOp)
    {
        switch (stencilOp)
        {
            case kGPUStencilOp_Keep:                return VK_STENCIL_OP_KEEP;
            case kGPUStencilOp_Zero:                return VK_STENCIL_OP_ZERO;
            case kGPUStencilOp_Replace:             return VK_STENCIL_OP_REPLACE;
            case kGPUStencilOp_IncrementAndClamp:   return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case kGPUStencilOp_DecrementAndClamp:   return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case kGPUStencilOp_Invert:              return VK_STENCIL_OP_INVERT;
            case kGPUStencilOp_IncrementAndWrap:    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case kGPUStencilOp_DecrementAndWrap:    return VK_STENCIL_OP_DECREMENT_AND_WRAP;

            default:
                UnreachableMsg("Unrecognised GPUStencilOp");
        }
    }

    inline VkBlendFactor ConvertBlendFactor(const GPUBlendFactor blendFactor)
    {
        switch (blendFactor)
        {
            case kGPUBlendFactor_Zero:                      return VK_BLEND_FACTOR_ZERO;
            case kGPUBlendFactor_One:                       return VK_BLEND_FACTOR_ONE;
            case kGPUBlendFactor_SrcColour:                 return VK_BLEND_FACTOR_SRC_COLOR;
            case kGPUBlendFactor_OneMinusSrcColour:         return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case kGPUBlendFactor_DstColour:                 return VK_BLEND_FACTOR_DST_COLOR;
            case kGPUBlendFactor_OneMinusDstColour:         return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case kGPUBlendFactor_SrcAlpha:                  return VK_BLEND_FACTOR_SRC_ALPHA;
            case kGPUBlendFactor_OneMinusSrcAlpha:          return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case kGPUBlendFactor_DstAlpha:                  return VK_BLEND_FACTOR_DST_ALPHA;
            case kGPUBlendFactor_OneMinusDstAlpha:          return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case kGPUBlendFactor_ConstantColour:            return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case kGPUBlendFactor_OneMinusConstantColour:    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case kGPUBlendFactor_ConstantAlpha:             return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case kGPUBlendFactor_OneMinusConstantAlpha:     return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            case kGPUBlendFactor_SrcAlphaSaturate:          return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;

            default:
                UnreachableMsg("Unrecognised GPUBlendFactor");

        }
    }

    inline VkBlendOp ConvertBlendOp(const GPUBlendOp blendOp)
    {
        switch (blendOp)
        {
            case kGPUBlendOp_Add:               return VK_BLEND_OP_ADD;
            case kGPUBlendOp_Subtract:          return VK_BLEND_OP_SUBTRACT;
            case kGPUBlendOp_ReverseSubtract:   return VK_BLEND_OP_REVERSE_SUBTRACT;
            case kGPUBlendOp_Min:               return VK_BLEND_OP_MIN;
            case kGPUBlendOp_Max:               return VK_BLEND_OP_MAX;

            default:
                UnreachableMsg("Unrecognised GPUBlendOp");

        }
    }

    inline VkFormat ConvertAttributeFormat(const GPUAttributeFormat format)
    {
        switch (format)
        {
            case kGPUAttributeFormat_R8_UNorm:              return VK_FORMAT_R8_UNORM;
            case kGPUAttributeFormat_R8G8_UNorm:            return VK_FORMAT_R8G8_UNORM;
            case kGPUAttributeFormat_R8G8B8_UNorm:          return VK_FORMAT_R8G8B8_UNORM;
            case kGPUAttributeFormat_R8G8B8A8_UNorm:        return VK_FORMAT_R8G8B8A8_UNORM;

            case kGPUAttributeFormat_R8_UInt:               return VK_FORMAT_R8_UINT;
            case kGPUAttributeFormat_R8G8_UInt:             return VK_FORMAT_R8G8_UINT;
            case kGPUAttributeFormat_R8G8B8_UInt:           return VK_FORMAT_R8G8B8_UINT;
            case kGPUAttributeFormat_R8G8B8A8_UInt:         return VK_FORMAT_R8G8B8A8_UINT;

            case kGPUAttributeFormat_R16_UNorm:             return VK_FORMAT_R16_UNORM;
            case kGPUAttributeFormat_R16G16_UNorm:          return VK_FORMAT_R16G16_UNORM;
            case kGPUAttributeFormat_R16G16B16_UNorm:       return VK_FORMAT_R16G16B16_UNORM;
            case kGPUAttributeFormat_R16G16B16A16_UNorm:    return VK_FORMAT_R16G16B16A16_UNORM;

            case kGPUAttributeFormat_R16_UInt:              return VK_FORMAT_R16_UINT;
            case kGPUAttributeFormat_R16G16_UInt:           return VK_FORMAT_R16G16_UINT;
            case kGPUAttributeFormat_R16G16B16_UInt:        return VK_FORMAT_R16G16B16_UINT;
            case kGPUAttributeFormat_R16G16B16A16_UInt:     return VK_FORMAT_R16G16B16A16_UINT;

            case kGPUAttributeFormat_R32_UInt:              return VK_FORMAT_R32_UINT;
            case kGPUAttributeFormat_R32G32_UInt:           return VK_FORMAT_R32G32_UINT;
            case kGPUAttributeFormat_R32G32B32_UInt:        return VK_FORMAT_R32G32B32_UINT;
            case kGPUAttributeFormat_R32G32B32A32_UInt:     return VK_FORMAT_R32G32B32A32_UINT;

            case kGPUAttributeFormat_R32_Float:             return VK_FORMAT_R32_SFLOAT;
            case kGPUAttributeFormat_R32G32_Float:          return VK_FORMAT_R32G32_SFLOAT;
            case kGPUAttributeFormat_R32G32B32_Float:       return VK_FORMAT_R32G32B32_SFLOAT;
            case kGPUAttributeFormat_R32G32B32A32_Float:    return VK_FORMAT_R32G32B32A32_SFLOAT;

            default:
                UnreachableMsg("Unrecognised GPUAttributeFormat");

        }
    }

    inline VkFilter ConvertFilter(const GPUFilter filter)
    {
        switch (filter)
        {
            case kGPUFilter_Nearest:    return VK_FILTER_NEAREST;
            case kGPUFilter_Linear:     return VK_FILTER_LINEAR;

            default:
                UnreachableMsg("Unrecognised GPUFilter");

        }
    }

    inline VkSamplerMipmapMode ConvertMipmapMode(const GPUFilter filter)
    {
        switch (filter)
        {
            case kGPUFilter_Nearest:    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case kGPUFilter_Linear:     return VK_SAMPLER_MIPMAP_MODE_LINEAR;

            default:
                UnreachableMsg("Unrecognised GPUFilter");

        }
    }

    inline VkSamplerAddressMode ConvertAddressMode(const GPUAddressMode addressMode)
    {
        switch (addressMode)
        {
            case kGPUAddressMode_Repeat:            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case kGPUAddressMode_MirroredRepeat:    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case kGPUAddressMode_Clamp:             return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case kGPUAddressMode_MirroredClamp:     return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

            default:
                UnreachableMsg("Unrecognised GPUAddressMode");

        }
    }
}
