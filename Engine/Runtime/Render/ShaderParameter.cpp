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

#include "Render/ShaderParameter.h"

bool ShaderParameter::IsConstant(const ShaderParameterType type)
{
    return !IsResource(type);
}

bool ShaderParameter::IsResource(const ShaderParameterType type)
{
    switch (type)
    {
        case kShaderParameterType_Texture2D:
        case kShaderParameterType_TextureCube:
            return true;

        default:
            return false;

    }
}

bool ShaderParameter::HasSampler(const ShaderParameterType type)
{
    /* Currently all resource types have a sampler. */
    return IsResource(type);
}

uint32_t ShaderParameter::GetSize(const ShaderParameterType type)
{
    Assert(IsConstant(type));

    switch (type)
    {
        case kShaderParameterType_Int:      return sizeof(shader_int);
        case kShaderParameterType_Int2:     return sizeof(shader_int2);
        case kShaderParameterType_Int3:     return sizeof(shader_int3);
        case kShaderParameterType_Int4:     return sizeof(shader_int4);
        case kShaderParameterType_UInt:     return sizeof(shader_uint);
        case kShaderParameterType_UInt2:    return sizeof(shader_uint2);
        case kShaderParameterType_UInt3:    return sizeof(shader_uint3);
        case kShaderParameterType_UInt4:    return sizeof(shader_uint4);
        case kShaderParameterType_Float:    return sizeof(shader_float);
        case kShaderParameterType_Float2:   return sizeof(shader_float2);
        case kShaderParameterType_Float3:   return sizeof(shader_float3);
        case kShaderParameterType_Float4:   return sizeof(shader_float4);

        default:
            UnreachableMsg("Invalid ShaderParameterType");

    }
}

const char* ShaderParameter::GetHLSLType(const ShaderParameterType type)
{
    switch (type)
    {
        case kShaderParameterType_Int:      return "int";
        case kShaderParameterType_Int2:     return "int2";
        case kShaderParameterType_Int3:     return "int3";
        case kShaderParameterType_Int4:     return "int4";
        case kShaderParameterType_UInt:     return "uint";
        case kShaderParameterType_UInt2:    return "uint2";
        case kShaderParameterType_UInt3:    return "uint3";
        case kShaderParameterType_UInt4:    return "uint4";
        case kShaderParameterType_Float:    return "float";
        case kShaderParameterType_Float2:   return "float2";
        case kShaderParameterType_Float3:   return "float3";
        case kShaderParameterType_Float4:   return "float4";

        default:
            UnreachableMsg("Invalid ShaderParameterType");

    }
}

GPUArgumentType ShaderParameter::GetGPUArgumentType(const ShaderParameterType type)
{
    Assert(IsResource(type));

    switch (type)
    {
        case kShaderParameterType_Texture2D:    return kGPUArgumentType_Texture;
        case kShaderParameterType_TextureCube:  return kGPUArgumentType_Texture;

        default:
            UnreachableMsg("Invalid ShaderParameterType");

    }
}
