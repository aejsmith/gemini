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

#include "Lighting.h"

/*
 * TODO:
 *  - Shader variants for when we are missing certain maps, e.g. emissive and
 *    occlusion. Currently supply dummy textures for these when they're
 *    unwanted, but we could remove the texture samples using variants.
 *  - Option to use mesh-provided tangents.
 */

struct VSInput
{
    float3      position        : POSITION;
    float3      normal          : NORMAL;
    float2      uv              : TEXCOORD;
};

struct PSInput
{
    float4      clipPosition    : SV_POSITION;
    float3      position        : POSITION;
    float3      normal          : NORMAL;
    float2      uv              : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.clipPosition = EntityPositionToClip(input.position);
    output.position     = EntityPositionToWorld(input.position);
    output.normal       = EntityNormalToWorld(input.normal);
    output.uv           = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    /* TODO. Use a single hardcoded point light source for now. */
    LightParams lightA;
    lightA.type            = kShaderLightType_Point;
    lightA.position        = float3(-4.0f, 2.0f, 3.0f);
    lightA.direction       = float3(0.0f, 0.0f, 0.0f);
    lightA.range           = 25.0f;
    lightA.colour          = float3(0.0f, 0.0f, 1.0f);
    lightA.intensity       = 100.0f;
    lightA.spotAngleScale  = 0.0f;
    lightA.spotAngleOffset = 0.0f;

    LightParams lightB;
    lightB.type            = kShaderLightType_Point;
    lightB.position        = float3(4.0f, 2.0f, 3.0f);
    lightB.direction       = float3(0.0f, 0.0f, 0.0f);
    lightB.range           = 25.0f;
    lightB.colour          = float3(0.0f, 1.0f, 0.0f);
    lightB.intensity       = 100.0f;
    lightB.spotAngleScale  = 0.0f;
    lightB.spotAngleOffset = 0.0f;

    /*
     * Fetch material parameters.
     */

    float4 baseColourSample        = MaterialSample(baseColourTexture, input.uv);
    float4 metallicRoughnessSample = MaterialSample(metallicRoughnessTexture, input.uv);
    float4 emissiveSample          = MaterialSample(emissiveTexture, input.uv);
    float4 normalSample            = MaterialSample(normalTexture, input.uv);
    float4 occlusionSample         = MaterialSample(occlusionTexture, input.uv);

    float3 emissive  = float3(emissiveSample) * emissiveFactor;
    float  occlusion = occlusionSample.r;

    MaterialParams material;
    material.baseColour = baseColourSample * baseColourFactor;
    material.metallic   = metallicRoughnessSample.b * metallicFactor;
    material.roughness  = metallicRoughnessSample.g * roughnessFactor;

    BRDFParams brdf = CalculateBRDFParams(material);

    /*
     * Calculate surface parameters.
     */

    float3 normal = PerturbNormal(input.normal,
                                  normalSample.xyz,
                                  input.position,
                                  input.uv);

    SurfaceParams surface = CalculateSurfaceParams(input.position, normal);

    /*
     * Calculate lighting.
     */

    float3 result = float3(0.0f, 0.0f, 0.0f);

    result += CalculateLight(lightA, brdf, surface);
    result += CalculateLight(lightB, brdf, surface);

    /* Apply occlusion. */
    result *= occlusion;

    /* Add emissive contribution. */
    result += emissive;

    return float4(result, baseColourSample.a);
}
