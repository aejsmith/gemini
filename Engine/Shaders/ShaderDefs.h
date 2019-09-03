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

/**
 * Compatibility types for code that needs to be shared between C++ and HLSL.
 * Unshared code should prefer the native types of the language.
 */
#if __HLSL__
    typedef float       shader_float;
    typedef float2      shader_float2;
    typedef float3      shader_float3;
    typedef float4      shader_float4;

    typedef int         shader_int;
    typedef int2        shader_int2;
    typedef int3        shader_int3;
    typedef int4        shader_int4;

    typedef uint        shader_uint;
    typedef uint2       shader_uint2;
    typedef uint3       shader_uint3;
    typedef uint4       shader_uint4;

    typedef bool        shader_bool;
    typedef bool2       shader_bool2;
    typedef bool3       shader_bool3;
    typedef bool4       shader_bool4;

    typedef float2x2    shader_float2x2;
    typedef float3x3    shader_float3x3;
    typedef float4x4    shader_float4x4;
#else
    typedef float       shader_float;
    typedef glm::vec2   shader_float2;
    typedef glm::vec3   shader_float3;
    typedef glm::vec4   shader_float4;

    typedef int         shader_int;
    typedef glm::ivec2  shader_int2;
    typedef glm::ivec3  shader_int3;
    typedef glm::ivec4  shader_int4;

    typedef uint        shader_uint;
    typedef glm::ivec2  shader_uint2;
    typedef glm::ivec3  shader_uint3;
    typedef glm::ivec4  shader_uint4;

    typedef bool        shader_bool;
    typedef glm::bvec2  shader_bool2;
    typedef glm::bvec3  shader_bool3;
    typedef glm::bvec4  shader_bool4;

    typedef glm::mat2   shader_float2x2;
    typedef glm::mat3   shader_float3x3;
    typedef glm::mat4   shader_float4x4;
#endif

/** Standard argument set indices. */
#define kArgumentSet_ViewEntity     0   /**< View/entity arguments. */
#define kArgumentSet_Material       1   /**< Material arguments. */

/**
 * Define a constant buffer. If the structure specified for this will be used
 * from C++ code, be careful regarding HLSL packing rules: in HLSL, members
 * will not straddle 16-byte boundaries, so it may be necessary to insert
 * explicit padding to ensure match between HLSL and C++.
 */
#ifdef __HLSL__
    #define CBUFFER(structName, name, setIndex, argumentIndex) \
        ConstantBuffer<structName> name : register(b ## argumentIndex, space ## setIndex)
#else
    #define CBUFFER(structName, name, setIndex, argumentIndex)
#endif

/**
 * View/Entity argument definitions.
 */

#define kViewEntityArguments_ViewConstants      0
#define kViewEntityArguments_EntityConstants    1

/** Standard view constants. */
struct ViewConstants
{
    shader_float4x4     viewMatrix;
    shader_float4x4     projectionMatrix;
    shader_float4x4     viewProjectionMatrix;
    shader_float4x4     inverseViewProjectionMatrix;
    shader_float3       position;
    shader_float        _pad0;
    shader_int2         targetSize;
};

CBUFFER(ViewConstants, view, kArgumentSet_ViewEntity, kViewEntityArguments_ViewConstants);

/** Standard entity constants. */
struct EntityConstants
{
    shader_float4x4     transformMatrix;
    shader_float3       position;
};

CBUFFER(EntityConstants, entity, kArgumentSet_ViewEntity, kViewEntityArguments_EntityConstants);
