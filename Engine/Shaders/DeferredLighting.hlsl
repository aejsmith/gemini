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

Texture2D                     GBuffer0Texture   : SRV(DeferredLighting, GBuffer0Texture);
Texture2D                     GBuffer1Texture   : SRV(DeferredLighting, GBuffer1Texture);
Texture2D                     GBuffer2Texture   : SRV(DeferredLighting, GBuffer2Texture);
Texture2D                     depthTexture      : SRV(DeferredLighting, DepthTexture);
StructuredBuffer<LightParams> lightParams       : SRV(DeferredLighting, LightParams);
StructuredBuffer<uint>        visibleLightCount : SRV(DeferredLighting, VisibleLightCount);
StructuredBuffer<uint>        visibleLights     : SRV(DeferredLighting, VisibleLights);
RWTexture2D<float4>           colourTexture     : UAV(DeferredLighting, ColourTexture);

CBUFFER(DeferredLightingConstants, constants, DeferredLighting, Constants);

groupshared uint tileLightIndices[kDeferredMaxLightCount];

[numthreads(kDeferredTileSize, kDeferredTileSize, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID,
            uint3 groupThreadID    : SV_GroupThreadID,
            uint3 groupID          : SV_GroupID)
{
    uint tileIndex   = (groupID.y * constants.tileDimensions.x) + groupID.x;
    uint threadIndex = (groupThreadID.y * kDeferredTileSize) + groupThreadID.x;
    uint threadCount = kDeferredTileSize * kDeferredTileSize;

    uint tileLightCount = visibleLightCount.Load(tileIndex);

    /* Early exit without bothering to read anything else if there's nothing
     * affecting this tile. */
    if (tileLightCount == 0)
    {
        return;
    }

    /* Read in and unpack the light list to LDS, the reverse of what the
     * culling shader did. Each thread reads 3 light indices. */
    uint elementCount     = (tileLightCount + 2) / 3;
    uint elementLoopCount = (elementCount + (threadCount - 1)) / threadCount;
    uint elementOffset    = tileIndex * kDeferredVisibleLightsTileEntryCount;

    for (uint i = 0; i < elementLoopCount; i++)
    {
        uint elementIndex = (i * threadCount) + threadIndex;

        if (elementIndex < elementCount)
        {
            uint element = visibleLights[elementOffset + elementIndex];
            uint light0  = element & 0x3ff;
            uint light1  = (element >> 10) & 0x3ff;
            uint light2  = (element >> 20) & 0x3ff;

            uint lightOffset = elementIndex * 3;

            tileLightIndices[lightOffset] = light0;

            /* Last 2 indices can go out of bounds. TODO: Check whether it'd be
             * better to just add a couple of extra entries on the end of the
             * array to avoid branching, same goes for culling shader. */
            if ((lightOffset + 1) < tileLightCount)
            {
                tileLightIndices[lightOffset + 1] = light1;
            }
            if ((lightOffset + 2) < tileLightCount)
            {
                tileLightIndices[lightOffset + 2] = light2;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    /* Sample and decode the G-Buffer. We read emissive (colourTexture) here as
     * well since we want to add our lighting result to that at the end. */
    uint2 targetPos = min(dispatchThreadID.xy, view.targetSize - 1);

    DeferredPSOutput psOutput;
    psOutput.target0 = GBuffer0Texture.Load(uint3(targetPos, 0));
    psOutput.target1 = GBuffer1Texture.Load(uint3(targetPos, 0));
    psOutput.target2 = GBuffer2Texture.Load(uint3(targetPos, 0));
    psOutput.target3 = colourTexture.Load(targetPos);
    float depth      = depthTexture.Load(uint3(targetPos, 0)).x;

    MaterialParams material;
    SurfaceParams surface;
    DecodeGBuffer(psOutput, depth, targetPos, material, surface);

    BRDFParams brdf = CalculateBRDFParams(material);

    float3 result = float3(0.0f);

    /* Iterate over each light. */
    for (uint i = 0; i < tileLightCount; i++)
    {
        LightParams light = lightParams.Load(tileLightIndices[i]);
        result += CalculateLight(light, brdf, surface);
    }

    /* Apply occlusion and add on emissive factor from the G-Buffer pass. */
    result *= material.occlusion;
    result += material.emissive;

    colourTexture[targetPos] = float4(result, 1.0);
}
