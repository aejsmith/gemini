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

#ifndef SHADERS_LIGHTINGDEFS_H
#define SHADERS_LIGHTINGDEFS_H

#include "ShaderDefs.h"

#define kShaderLightType_Directional    0
#define kShaderLightType_Point          1
#define kShaderLightType_Spot           2

struct LightParams
{
    /** World space position. */
    shader_float3   position;

    /** Light type (kShaderLightType_*). */
    shader_int      type;

    /** World space direction (for directional/spot). */
    shader_float3   direction;

    /** Range of the light (for point/spot). */
    shader_float    range;

    /** RGB colour of the light. */
    shader_float3   colour;

    /** Intensity (interpreted according to the light type). */
    shader_float    intensity;

    /** Spotlight attenuation parameters (derived from cone angles). */
    shader_float    spotAngleScale;
    shader_float    spotAngleOffset;

    shader_float2   _pad0;
};

#endif /* SHADERS_LIGHTINGDEFS_H */