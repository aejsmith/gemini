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

#include "Deferred.h"

Texture2D                     depthTexture      : SRV(DeferredCulling, DepthTexture);
StructuredBuffer<LightParams> lightParams       : SRV(DeferredCulling, LightParams);
RWStructuredBuffer<uint>      visibleLightCount : UAV(DeferredCulling, VisibleLightCount);
RWStructuredBuffer<uint>      visibleLights     : UAV(DeferredCulling, VisibleLights);

CBUFFER(DeferredCullingConstants, constants, DeferredCulling, Constants);

groupshared uint tileMinDepth;
groupshared uint tileMaxDepth;

groupshared uint tileLightCount;
groupshared uint tileLightIndices[kDeferredMaxLightCount];

bool IsLightVisible(float minDepth,
                    float maxDepth,
                    uint  lightIndex)
{
    LightParams light = lightParams.Load(lightIndex);

    /* TODO: Actually cull the lights... */

    return true;
}

[numthreads(kDeferredTileSize, kDeferredTileSize, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID,
            uint3 groupThreadID    : SV_GroupThreadID,
            uint3 groupID          : SV_GroupID)
{
    uint tileIndex   = (groupID.y * constants.tileDimensions.x) + groupID.x;
    uint threadIndex = (groupThreadID.y * kDeferredTileSize) + groupThreadID.x;

    /* Initialise shared memory state. */
    if (threadIndex == 0)
    {
        tileMinDepth   = 0x7f7fffff; /* FLT_MAX */
        tileMaxDepth   = 0;
        tileLightCount = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    /* Clamp to target size in case the target is not a multiple of tile
     * dimensions. */
    uint2 targetPos = min(dispatchThreadID.xy, view.targetSize);
    float depth     = depthTexture.Load(uint3(targetPos, 0)).x;

    /* Compute min/max depth for the tile. TODO: Subgroup-based implementation. */
    uint depthInt = asuint(depth);
    InterlockedMin(tileMinDepth, depthInt);
    InterlockedMax(tileMaxDepth, depthInt);

    GroupMemoryBarrierWithGroupSync();

    float minDepth = asfloat(tileMinDepth);
    float maxDepth = asfloat(tileMaxDepth);

    /* Iterate over lights. Each thread processes a single light. */
    uint threadCount    = kDeferredTileSize * kDeferredTileSize;
    uint lightCount     = constants.lightCount;
    uint lightLoopCount = (lightCount + (threadCount - 1)) / threadCount;

    for (uint i = 0; i < lightLoopCount; i++)
    {
        uint lightIndex = (i * threadCount) + threadIndex;

        /* Since we rounded up the light count to a multiple of the group size,
         * threads in the last iteration may be outside the count. */
        if (lightIndex < lightCount)
        {
            if (IsLightVisible(minDepth, maxDepth, lightIndex))
            {
                uint outputIndex;
                InterlockedAdd(tileLightCount, 1, outputIndex);
                tileLightIndices[outputIndex] = lightIndex;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    /* Export the visible light list to the buffers. This buffer has 3 10-bit
     * light indices packed into each 32-bit entry. Each thread will export 3
     * lights. */
    if (threadIndex == 0)
    {
        visibleLightCount[tileIndex] = tileLightCount;
    }

    uint exportCount     = (tileLightCount + 2) / 3;
    uint exportLoopCount = (exportCount + (threadCount - 1)) / threadCount;
    uint exportOffset    = tileIndex * kDeferredVisibleLightsTileEntryCount;

    for (uint i = 0; i < exportLoopCount; i++)
    {
        uint exportIndex = (i * threadCount) + threadIndex;

        if (exportIndex < exportCount)
        {
            /* Last 2 indices can go out of bounds of the light array. */
            uint lightOffset = exportIndex * 3;
            uint light0      = tileLightIndices[lightOffset];
            uint light1      = ((lightOffset + 1) < tileLightCount) ? tileLightIndices[lightOffset + 1] : 0;
            uint light2      = ((lightOffset + 2) < tileLightCount) ? tileLightIndices[lightOffset + 2] : 0;

            visibleLights[exportOffset + exportIndex] = (light2 << 20) | (light1 << 10) | light0;
        }
    }
}
