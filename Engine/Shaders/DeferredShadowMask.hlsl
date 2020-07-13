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

#define ENABLE_PCF 1

Texture2D depthTexture                  : SRV(DeferredShadowMask, DepthTexture);
Texture2D shadowMapTexture              : SRV(DeferredShadowMask, ShadowMapTexture);
Texture2DArray shadowMapArrayTexture    : SRV(DeferredShadowMask, ShadowMapTexture);
SamplerComparisonState shadowMapSampler : SMP(DeferredShadowMask, ShadowMapSampler);

CBUFFER(DeferredShadowMaskConstants, constants, DeferredShadowMask, Constants);

float SampleShadowMap(float3 surfacePos, uint shadowView)
{
    /* Transform to shadow map UV coordinates. */
    float4 shadowPos  = mul(constants.worldToShadowMatrix[shadowView], float4(surfacePos, 1.0f));
    float2 shadowUV   = ((shadowPos.xy / shadowPos.w) * float2(0.5f, -0.5f)) + 0.5f;
    float shadowDepth = (shadowPos.z / shadowPos.w) - constants.biasConstant;

    #if ENABLE_PCF
        /* 3x3 PCF. */
        float attenuation = 0.0f;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                attenuation += shadowMapTexture.SampleCmp(shadowMapSampler, shadowUV, shadowDepth, int2(x, y));
            }
        }

        attenuation /= 9.0f;
        return attenuation;
    #else
        return shadowMapTexture.SampleCmp(shadowMapSampler, shadowUV, shadowDepth);
    #endif
}

float SampleShadowMapArray(float3 surfacePos, uint shadowView)
{
    /* Transform to shadow map UV coordinates. */
    float4 shadowPos  = mul(constants.worldToShadowMatrix[shadowView], float4(surfacePos, 1.0f));
    float3 shadowUV   = float3(((shadowPos.xy / shadowPos.w) * float2(0.5f, -0.5f)) + 0.5f, shadowView);
    float shadowDepth = (shadowPos.z / shadowPos.w) - constants.biasConstant;

    #if ENABLE_PCF
        /* 3x3 PCF. */
        float attenuation = 0.0f;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                attenuation += shadowMapArrayTexture.SampleCmp(shadowMapSampler, shadowUV, shadowDepth, int2(x, y));
            }
        }

        attenuation /= 9.0f;
        return attenuation;
    #else
        return shadowMapArrayTexture.SampleCmp(shadowMapSampler, shadowUV, shadowDepth);
    #endif
}

/**
 * Directional
 */

float4 VSDirectional(const uint vertexID : SV_VertexID) : SV_Position
{
    float4 position;

    /* Fullscreen triangle. Set depth as the maximum shadow draw distance so
     * that we'll pass depth test only for pixels closer than that. */
    float4 clipDepth = mul(view.projection, float4(0.0f, 0.0f, -constants.directionalMaxDistance, 1.0f));

    position.x = ((float)(vertexID % 2) * 4.0f) - 1.0f;
    position.y = ((float)(vertexID / 2) * 4.0f) - 1.0f;
    position.z = min(clipDepth.z / clipDepth.w, 1.0f);
    position.w = 1.0f;

    return position;
}

uint SelectCascade(float3 viewPos)
{
    float viewDepth = -viewPos.z;

    if (viewDepth < constants.directionalSplitDepths.x)
    {
        return 0;
    }
    else if (viewDepth < constants.directionalSplitDepths.y)
    {
        return 1;
    }
    else if (viewDepth < constants.directionalSplitDepths.z)
    {
        return 2;
    }
    else
    {
        return 3;
    }
}

[earlydepthstencil]
float4 PSDirectional(float4 position : SV_Position) : SV_Target0
{
    /* First we calculate view space position of the pixel from depth, which
     * we use to select a cascade from the split depths (which are in view
     * space). */
    uint2 targetPos = position.xy;
    float depth     = depthTexture.Load(uint3(targetPos, 0)).x;
    float3 viewPos  = ViewPixelPositionToView(targetPos, depth);

    uint cascade = SelectCascade(viewPos);

    float3 surfacePos = mul(view.inverseView, float4(viewPos, 1.0));

    float attenuation = SampleShadowMapArray(surfacePos, cascade);
    return float4(attenuation, 0.0f, 0.0f, 0.0f);
}

/**
 * Spot
 */

float4 VSSpot(float3 position : POSITION) : SV_Position
{
    return EntityPositionToClip(position);
}

[earlydepthstencil]
float4 PSSpot(float4 position : SV_Position) : SV_Target0
{
    /* Calculate world space position of this pixel from depth. */
    uint2 targetPos   = position.xy;
    float depth       = depthTexture.Load(uint3(targetPos, 0)).x;
    float3 surfacePos = ViewPixelPositionToWorld(targetPos, depth);
    float3 toSurface  = surfacePos - constants.position;
    float distance    = length(toSurface);

    /* Early out if this pixel is outside the light volume (our depth testing
     * is imperfect and won't reject pixels in front and outside of the light
     * volume. */
    if (distance > constants.range ||
        dot(constants.direction, normalize(toSurface)) < constants.cosSpotAngle)
    {
        discard;
    }

    float attenuation = SampleShadowMap(surfacePos, 0);
    return float4(attenuation, 0.0f, 0.0f, 0.0f);
}
