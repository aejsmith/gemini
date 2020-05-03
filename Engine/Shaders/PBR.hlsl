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

#include "Deferred.h"
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
    float4      clipPosition    : SV_Position;
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

float4 PSBasic(PSInput input) : SV_Target0
{
    /* Basic pass has no lighting, just output the base colour. */
    return float4(MaterialSample(baseColourTexture, input.uv).xyz, 1.0f);
}

DeferredPSOutput PSDeferredOpaque(PSInput input)
{
    float4 baseColourSample        = MaterialSample(baseColourTexture, input.uv);
    float4 metallicRoughnessSample = MaterialSample(metallicRoughnessTexture, input.uv);
    float4 emissiveSample          = MaterialSample(emissiveTexture, input.uv);
    float4 normalSample            = MaterialSample(normalTexture, input.uv);
    float4 occlusionSample         = MaterialSample(occlusionTexture, input.uv);

    MaterialParams material;
    material.baseColour = baseColourSample * baseColourFactor;
    material.metallic   = metallicRoughnessSample.b * metallicFactor;
    material.roughness  = metallicRoughnessSample.g * roughnessFactor;
    material.emissive   = float3(emissiveSample) * emissiveFactor;
    material.occlusion  = occlusionSample.r;

    float3 normal = PerturbNormal(input.normal,
                                  normalSample.xyz,
                                  input.position,
                                  input.uv);

    return EncodeGBuffer(material, normal);
}
