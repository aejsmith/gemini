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

/**
 * Compatibility types for code that needs to be shared between C++ and HLSL.
 * Unshared code should prefer the native types of the language.
 */
#if __HLSL__
    typedef float           shader_float;
    typedef float2          shader_float2;
    typedef float3          shader_float3;
    typedef float4          shader_float4;

    typedef int             shader_int;
    typedef int2            shader_int2;
    typedef int3            shader_int3;
    typedef int4            shader_int4;

    typedef uint            shader_uint;
    typedef uint2           shader_uint2;
    typedef uint3           shader_uint3;
    typedef uint4           shader_uint4;

    typedef bool            shader_bool;
    typedef bool2           shader_bool2;
    typedef bool3           shader_bool3;
    typedef bool4           shader_bool4;

    typedef float2x2        shader_float2x2;
    typedef float3x3        shader_float3x3;
    typedef float4x4        shader_float4x4;
#else
    typedef float           shader_float;
    typedef glm::vec2       shader_float2;
    typedef glm::vec3       shader_float3;
    typedef glm::vec4       shader_float4;

    typedef int             shader_int;
    typedef glm::ivec2      shader_int2;
    typedef glm::ivec3      shader_int3;
    typedef glm::ivec4      shader_int4;

    typedef unsigned int    shader_uint;
    typedef glm::ivec2      shader_uint2;
    typedef glm::ivec3      shader_uint3;
    typedef glm::ivec4      shader_uint4;

    typedef bool            shader_bool;
    typedef glm::bvec2      shader_bool2;
    typedef glm::bvec3      shader_bool3;
    typedef glm::bvec4      shader_bool4;

    typedef glm::mat2       shader_float2x2;
    typedef glm::mat3       shader_float3x3;
    typedef glm::mat4       shader_float4x4;
#endif

#define SHADER_PI 3.14159265f

/** Standard argument set indices. */
#define kArgumentSet_ViewEntity     0   /**< View/entity arguments. */
#define kArgumentSet_Material       1   /**< Material arguments. */

/**
 * Macros for HLSL register specifiers.
 *
 * The index forms take a set and argument index (can come from definitions).
 * 
 * The name forms are helpers to shorten use of set/argument index defines
 * named in the standard form: SRV(MySet, MyArgument) expands to use the set
 * index defined by kArgumentSet_MySet, and the argument index defined bt
 * kMySetArguments_MyArgument.
 */
#if __HLSL__
    #define SRV_INDEX(setIndex, argumentIndex) \
        register(t ## argumentIndex, space ## setIndex)
    #define SMP_INDEX(setIndex, argumentIndex) \
        register(s ## argumentIndex, space ## setIndex)
    #define UAV_INDEX(setIndex, argumentIndex) \
        register(u ## argumentIndex, space ## setIndex)

    #define SET_DEFINE(setName) \
        kArgumentSet_ ## setName
    #define ARGUMENT_DEFINE(setName, argumentName) \
        k ## setName ## Arguments_ ## argumentName

    #define SRV(setName, argumentName) \
        SRV_INDEX(SET_DEFINE(setName), ARGUMENT_DEFINE(setName, argumentName))
    #define SMP(setName, argumentName) \
        SMP_INDEX(SET_DEFINE(setName), ARGUMENT_DEFINE(setName, argumentName))
    #define UAV(setName, argumentName) \
        UAV_INDEX(SET_DEFINE(setName), ARGUMENT_DEFINE(setName, argumentName))
#endif

/**
 * Define a constant buffer. If the structure specified for this will be used
 * from C++ code, be careful regarding HLSL packing rules: in HLSL, members
 * will not straddle 16-byte boundaries, so it may be necessary to insert
 * explicit padding to ensure match between HLSL and C++.
 */
#if __HLSL__
    #define CBUFFER_INDEX(structName, name, setIndex, argumentIndex) \
        ConstantBuffer<structName> name : register(b ## argumentIndex, space ## setIndex)
    #define CBUFFER(structName, name, setName, argumentName) \
        CBUFFER_INDEX(structName, name, SET_DEFINE(setName), ARGUMENT_DEFINE(setName, argumentName))
#else
    #define CBUFFER_INDEX(structName, name, setIndex, argumentIndex)
    #define CBUFFER(structName, name, setName, argumentName)
#endif

/**
 * View/Entity argument definitions.
 */

#define kViewEntityArguments_ViewConstants      0
#define kViewEntityArguments_EntityConstants    1
#define kViewEntityArgumentCount                2

/** Standard view constants. */
struct ViewConstants
{
    shader_float4x4     view;
    shader_float4x4     projection;
    shader_float4x4     viewProjection;
    shader_float4x4     inverseView;
    shader_float4x4     inverseProjection;
    shader_float4x4     inverseViewProjection;
    shader_float3       position;
    shader_float        _pad0;
    shader_float        zNear;
    shader_float        zFar;
    shader_uint2        targetSize;
};

CBUFFER(ViewConstants, view, ViewEntity, ViewConstants);

/** Standard entity constants. */
struct EntityConstants
{
    shader_float4x4     transform;
    shader_float3       position;
};

CBUFFER(EntityConstants, entity, ViewEntity, EntityConstants);

#if __HLSL__

/**
 * Entity helper functions/definitions.
 */

float4 EntityPositionToClip(float4 position)
{
    return mul(view.viewProjection, mul(entity.transform, position));
}

float4 EntityPositionToClip(float3 position)
{
    return EntityPositionToClip(float4(position, 1.0));
}

float3 EntityPositionToWorld(float3 position)
{
    return mul(entity.transform, float4(position, 1.0)).xyz;
}

float3 EntityNormalToWorld(float3 normal)
{
    /* Set W component to 0 to remove translation. */
    return mul(entity.transform, float4(normal, 0.0)).xyz;
}

/**
 * View helper functions/definitions.
 */

/**
 * Linearise a depth buffer value according to the current view. Returns linear
 * view-space depth in the range [zNear, zFar].
 */
float ViewLineariseDepth(float depth)
{
    float n = view.zNear;
    float f = view.zFar;
    return (f * n) / (f + (depth * (n - f)));
}

/** Transform a NDC position for the current view to a world-space position. */
float3 ViewNDCPositionToWorld(float4 ndcPos)
{
    float4 homogeneousPos = mul(view.inverseViewProjection, ndcPos);
    return homogeneousPos.xyz / homogeneousPos.w;
}

float3 ViewNDCPositionToWorld(float3 ndcPos)
{
    return ViewNDCPositionToWorld(float4(ndcPos, 1.0f));
}

/** Transform a NDC position for the current view to a view-space position. */
float3 ViewNDCPositionToView(float4 ndcPos)
{
    float4 homogeneousPos = mul(view.inverseProjection, ndcPos);
    return homogeneousPos.xyz / homogeneousPos.w;
}

float3 ViewNDCPositionToView(float3 ndcPos)
{
    return ViewNDCPositionToView(float4(ndcPos, 1.0f));
}

/**
 * Given an integer pixel coordinate on the current view, return corresponding
 * NDC X/Y coordinates.
 */
float2 ViewPixelPositionToNDC(uint2 targetPos)
{
    return ((float2(targetPos.x, view.targetSize.y - targetPos.y) / view.targetSize) * 2.0f) - 1.0f;
}

/**
 * Given an integer pixel coordinate on the current view and a depth value,
 * return corresponding NDC coordinates.
 */
float4 ViewPixelPositionToNDC(uint2 targetPos, float depth)
{
    return float4(ViewPixelPositionToNDC(targetPos), depth, 1.0f);
}

/**
 * Given an integer pixel coordinate on the current view and a depth value,
 * return corresponding world-space coordinates.
 */
float3 ViewPixelPositionToWorld(uint2 targetPos, float depth)
{
    return ViewNDCPositionToWorld(ViewPixelPositionToNDC(targetPos, depth));
}

/**
 * Given an integer pixel coordinate on the current view and a depth value,
 * return corresponding view-space coordinates.
 */
float3 ViewPixelPositionToView(uint2 targetPos, float depth)
{
    return ViewNDCPositionToView(ViewPixelPositionToNDC(targetPos, depth));
}

/**
 * Material helper functions/definitions.
 */

/**
 * Sample from a material texture. This is a convenience wrapper to sample
 * a material texture with its associated sampler. The texture object is named
 * <ParameterName>_texture, and the sampler is named <ParameterName>_sampler.
 * Parameters to this function are as for HLSL's Sample(), minus the first
 * sampler argument.
 */
#define MaterialSample(parameterName, location) \
    parameterName ## _texture.Sample(parameterName ## _sampler, location)

#endif /* __HLSL__ */
