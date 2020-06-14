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
#include "ShadowMap.h"

/*
 * TODO:
 *  - Option to use mesh-provided tangents. Needs support for variants based
 *    on the mesh used with a material.
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

struct VSShadowMapInput
{
    float3      position        : POSITION;

    #if ALPHA_TEST
    float2      uv              : TEXCOORD;
    #endif
};

struct PSShadowMapInput
{
    float4      shadowPosition  : SV_Position;

    #if ALPHA_TEST
    float2      uv              : TEXCOORD;
    #endif
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.clipPosition = EntityPositionToClip(input.position);
    output.position     = EntityPositionToWorld(input.position);
    output.normal       = EntityNormalToWorld(input.normal);
    output.uv           = input.uv * uvScale;
    return output;
}

PSShadowMapInput VSShadowMap(VSShadowMapInput input)
{
    PSShadowMapInput output;
    output.shadowPosition = ShadowMapPosition(input.position);

    #if ALPHA_TEST
        output.uv = input.uv;
    #endif

    return output;
}

float3 GetBaseColour(float2 uv)
{
    float4 baseColour = MaterialSample(baseColourTexture, uv) * baseColourFactor;

    #if ALPHA_TEST
        if (baseColour.a < alphaCutoff)
        {
            discard;
        }
    #endif

    return baseColour.rgb;
}

float4 PSBasic(PSInput input) : SV_Target0
{
    /* Basic pass has no lighting, just output the base colour. */
    return float4(GetBaseColour(input.uv), 1.0f);
}

DeferredPSOutput PSDeferredOpaque(PSInput input)
{
    float3 baseColour              = GetBaseColour(input.uv);
    float4 metallicRoughnessSample = MaterialSample(metallicRoughnessTexture, input.uv);
    float4 normalSample            = MaterialSample(normalTexture, input.uv);

    #if OCCLUSION
        float4 occlusionSample     = MaterialSample(occlusionTexture, input.uv);
    #endif

    #if EMISSIVE
        float4 emissiveSample      = MaterialSample(emissiveTexture, input.uv);
    #endif

    MaterialParams material;
    material.baseColour = baseColour;
    material.metallic   = metallicRoughnessSample.b * metallicFactor;
    material.roughness  = metallicRoughnessSample.g * roughnessFactor;

    #if OCCLUSION
        material.occlusion = occlusionSample.r;
    #else
        material.occlusion = 1.0f;
    #endif

    #if EMISSIVE
        material.emissive = float3(emissiveSample) * emissiveFactor;
    #else
        material.emissive = float3(0.0f);
    #endif

    float3 normal = PerturbNormal(input.normal,
                                  ((normalSample.xyz * 2.0f) - 1.0f) * normalScale,
                                  input.position,
                                  input.uv);

    return EncodeGBuffer(material, normal);
}

#if ALPHA_TEST

void PSShadowMap(PSShadowMapInput input)
{
    /* Does the alpha test and discards if fails. We don't have any colour
     * output in this pass. */
    GetBaseColour(input.uv);
}

#endif
