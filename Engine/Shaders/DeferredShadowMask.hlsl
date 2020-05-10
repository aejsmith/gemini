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
SamplerComparisonState shadowMapSampler : SMP(DeferredShadowMask, ShadowMapSampler);    

CBUFFER(DeferredShadowMaskConstants, constants, DeferredShadowMask, Constants);

float4 VSMain(float3 position : POSITION) : SV_Position
{
    return EntityPositionToClip(position);
}

[earlydepthstencil]
float4 PSSpotLight(float4 position : SV_Position) : SV_Target0
{
    /* Calculate world-space position of this pixel from depth. */
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

    /* Transform to shadow map UV coordinates. */
    float4 shadowPos  = mul(constants.worldToShadowMatrix, float4(surfacePos, 1.0f));
    float2 shadowUV   = ((shadowPos.xy / shadowPos.w) * float2(0.5f, -0.5f)) + 0.5f;
    float shadowDepth = (shadowPos.z / shadowPos.w) - constants.biasConstant;

    #if ENABLE_PCF
        float width, height;
        shadowMapTexture.GetDimensions(width, height);
        float2 texelSize = 1.0f / float2(width, height);

        /* 3x3 PCF. */
        float attenuation = 0.0f;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                float2 uv = shadowUV + (float2(x, y) * texelSize);
                attenuation += shadowMapTexture.SampleCmp(shadowMapSampler, uv, shadowDepth);
            }
        }

        attenuation /= 9.0f;
    #else
        float attenuation = shadowMapTexture.SampleCmp(shadowMapSampler, shadowUV, shadowDepth);
    #endif

    return float4(attenuation, 0.0f, 0.0f, 0.0f);
}
