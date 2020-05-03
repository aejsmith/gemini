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

#pragma once

#include "Engine/Object.h"

#include "Render/RenderDefs.h"

class Texture2D;

/** Types of parameters for a shader technique. */
enum ENUM() ShaderParameterType
{
    /**
     * Constant value types, directed to the material constant buffer.
     */
    kShaderParameterType_Int,               /**< Signed 32-bit integer. */
    kShaderParameterType_Int2,              /**< 2 component signed 32-bit integer vector. */
    kShaderParameterType_Int3,              /**< 3 component signed 32-bit integer vector. */
    kShaderParameterType_Int4,              /**< 4 component signed 32-bit integer vector. */
    kShaderParameterType_UInt,              /**< Unsigned 32-bit integer. */
    kShaderParameterType_UInt2,             /**< 2 component unsigned 32-bit integer vector. */
    kShaderParameterType_UInt3,             /**< 3 component unsigned 32-bit integer vector. */
    kShaderParameterType_UInt4,             /**< 4 component unsigned 32-bit integer vector. */
    kShaderParameterType_Float,             /**< Single-precision floating point. */
    kShaderParameterType_Float2,            /**< 2 component single-precision floating point vector. */
    kShaderParameterType_Float3,            /**< 3 component single-precision floating point vector. */
    kShaderParameterType_Float4,            /**< 4 component single-precision floating point vector. */

    /**
     * Resource types.
     */
    kShaderParameterType_Texture2D,         /**< 2D texture. */

    kShaderParameterTypeCount
};

struct ShaderParameter
{
    std::string                     name;
    ShaderParameterType             type;

    union
    {
        /** For constants, offset in the material constant buffer. */
        uint32_t                    constantOffset;

        /**
         * For resources, index in the material argument set. For types that
         * have a sampler, the sampler argument index is (argumentIndex + 1).
         */
        uint32_t                    argumentIndex;
    };

public:
    static bool                     IsConstant(const ShaderParameterType type);
    static bool                     IsResource(const ShaderParameterType type);
    static bool                     HasSampler(const ShaderParameterType type);
    static uint32_t                 GetSize(const ShaderParameterType type);
    static const char*              GetHLSLType(const ShaderParameterType type);
    static GPUArgumentType          GetGPUArgumentType(const ShaderParameterType type);

};

/** Mapping from a C++ type to a ShaderParameterType enum. */
template <typename T>
struct ShaderParameterTypeTraits;

#define SHADER_PARAMETER_TYPE_TRAIT(Type, Enum) \
    template <> \
    struct ShaderParameterTypeTraits<Type> \
    { \
        static constexpr ShaderParameterType kType = Enum; \
    }

SHADER_PARAMETER_TYPE_TRAIT(int32_t,            kShaderParameterType_Int);
SHADER_PARAMETER_TYPE_TRAIT(glm::ivec2,         kShaderParameterType_Int2);
SHADER_PARAMETER_TYPE_TRAIT(glm::ivec3,         kShaderParameterType_Int3);
SHADER_PARAMETER_TYPE_TRAIT(glm::ivec4,         kShaderParameterType_Int4);
SHADER_PARAMETER_TYPE_TRAIT(uint32_t,           kShaderParameterType_UInt);
SHADER_PARAMETER_TYPE_TRAIT(glm::uvec2,         kShaderParameterType_UInt2);
SHADER_PARAMETER_TYPE_TRAIT(glm::uvec3,         kShaderParameterType_UInt3);
SHADER_PARAMETER_TYPE_TRAIT(glm::uvec4,         kShaderParameterType_UInt4);
SHADER_PARAMETER_TYPE_TRAIT(float,              kShaderParameterType_Float);
SHADER_PARAMETER_TYPE_TRAIT(glm::vec2,          kShaderParameterType_Float2);
SHADER_PARAMETER_TYPE_TRAIT(glm::vec3,          kShaderParameterType_Float3);
SHADER_PARAMETER_TYPE_TRAIT(glm::vec4,          kShaderParameterType_Float4);
SHADER_PARAMETER_TYPE_TRAIT(ObjPtr<Texture2D>,  kShaderParameterType_Texture2D);

#undef SHADER_PARAMETER_TYPE_TRAIT
