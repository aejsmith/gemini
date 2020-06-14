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

Texture2D                     depthTexture      : SRV(DeferredCulling, DepthTexture);
StructuredBuffer<LightParams> lightParams       : SRV(DeferredCulling, LightParams);
RWStructuredBuffer<uint>      visibleLightCount : UAV(DeferredCulling, VisibleLightCount);
RWStructuredBuffer<uint>      visibleLights     : UAV(DeferredCulling, VisibleLights);

CBUFFER(DeferredCullingConstants, constants, DeferredCulling, Constants);

groupshared uint tileMinDepth;
groupshared uint tileMaxDepth;

groupshared uint tileLightCount;
groupshared uint tileLightIndices[kDeferredMaxLightCount];

#define kPlane_Left     0
#define kPlane_Right    1
#define kPlane_Top      2
#define kPlane_Bottom   3
#define kPlane_Near     4
#define kPlane_Far      5
#define kNumPlanes      6

struct Frustum
{
    float4 planes[kNumPlanes];
};

/**
 * Calculate a plane (normal, distance) from 3 points forming a triangle on the
 * plane (clockwise winding, the normal will face away from the plane).
 */
float4 CalculatePlane(float3 p0, float3 p1, float3 p2)
{
    float3 normal  = normalize(cross(p1 - p0, p2 - p0));
    float distance = dot(normal, p0);
    return float4(normal, distance);
}

void CalculateTileFrustum(uint2       tileID,
                          float       minDepth,
                          float       maxDepth,
                          out Frustum frustum)
{
    /*
     * The world-space corners of the frustum are obtained by transforming the
     * NDC coordinates for the tile, limited to the minimum and maximum depth
     * range, by the inverse view-projection transform. From these we can
     * construct the frustum planes used for culling.
     *
     * This is actually simplified a little bit so that we don't need to do a
     * full calculation of all 8 frustum corners. We can just use the far plane
     * corners combined with the eye position to get all the planes except the
     * near plane. We then just construct the near plane by inverting the far
     * plane and then moving it based on the difference between the (linear)
     * minimum and maximum depth.
     */

    /* Bottom right may go outside the target if its size is not a multiple of
     * the tile size, clamp to avoid issues. */
    float2 tileTopLeft     = ViewPixelPositionToNDC(tileID * kDeferredTileSize);
    float2 tileBottomRight = ViewPixelPositionToNDC(min((tileID + 1) * kDeferredTileSize, view.targetSize));

    /* T = Top, B = Bottom, L = Left, R = Right. */
    float3 farCorners[4];
    farCorners[0] /* TL */ = ViewNDCPositionToWorld(float3(tileTopLeft, maxDepth));
    farCorners[1] /* TR */ = ViewNDCPositionToWorld(float3(tileBottomRight.x, tileTopLeft.y, maxDepth));
    farCorners[2] /* BL */ = ViewNDCPositionToWorld(float3(tileTopLeft.x, tileBottomRight.y, maxDepth));
    farCorners[3] /* BR */ = ViewNDCPositionToWorld(float3(tileBottomRight, maxDepth));

    frustum.planes[kPlane_Left]   = CalculatePlane(farCorners[0], view.position, farCorners[2]);
    frustum.planes[kPlane_Right]  = CalculatePlane(farCorners[3], view.position, farCorners[1]);
    frustum.planes[kPlane_Top]    = CalculatePlane(farCorners[1], view.position, farCorners[0]);
    frustum.planes[kPlane_Bottom] = CalculatePlane(farCorners[2], view.position, farCorners[3]);
    frustum.planes[kPlane_Far]    = CalculatePlane(farCorners[1], farCorners[2], farCorners[3]);

    /* Near plane is obtained by inverting the far plane and adjusting the
     * distance by the difference between the linearised maximum and minimum
     * depth. */
    float linearMinDepth = ViewLineariseDepth(minDepth);
    float linearMaxDepth = ViewLineariseDepth(maxDepth);
    frustum.planes[kPlane_Near] = -frustum.planes[kPlane_Far];
    frustum.planes[kPlane_Near].w -= linearMaxDepth - linearMinDepth;
}

float DistanceToPlane(float3 p, float4 plane)
{
    return dot(plane.xyz, p) - plane.w;
}

bool IsLightVisible(Frustum frustum,
                    uint    lightIndex)
{
    LightParams light = lightParams.Load(lightIndex);

    /* Directional lights are always visible. */
    bool visible = true;

    if (light.type != kShaderLightType_Directional && light.range != 0.0f)
    {
        /* Sphere/frustum intersection test. */
        float3 position = light.boundingSphere.xyz;
        float dist      = -light.boundingSphere.w;

        [unroll]
        for (uint i = 0; i < kNumPlanes; i++)
        {
            visible = visible && DistanceToPlane(position, frustum.planes[i]) >= dist;
        }
    }

    return visible;
}

[numthreads(kDeferredTileSize, kDeferredTileSize, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID,
            uint3 groupThreadID    : SV_GroupThreadID,
            uint3 groupID          : SV_GroupID)
{
    uint2 tileID     = groupID.xy;
    uint tileIndex   = (tileID.y * constants.tileDimensions.x) + tileID.x;
    uint threadIndex = (groupThreadID.y * kDeferredTileSize) + groupThreadID.x;
    uint threadCount = kDeferredTileSize * kDeferredTileSize;

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
    uint2 targetPos = min(dispatchThreadID.xy, view.targetSize - 1);
    float depth     = depthTexture.Load(uint3(targetPos, 0)).x;

    /* Compute min/max depth for the tile. */
    float waveMinDepth = WaveActiveMin(depth);
    float waveMaxDepth = WaveActiveMax(depth);

    float minDepth;
    float maxDepth;

    if (WaveGetLaneCount() == kDeferredTileSize * kDeferredTileSize)
    {
        minDepth = waveMinDepth;
        maxDepth = waveMaxDepth;
    }
    else
    {
        if (WaveIsFirstLane())
        {
            uint depthInt = asuint(depth);
            InterlockedMin(tileMinDepth, asuint(waveMinDepth));
            InterlockedMax(tileMaxDepth, asuint(waveMaxDepth));
        }

        GroupMemoryBarrierWithGroupSync();

        minDepth = asfloat(tileMinDepth);
        maxDepth = asfloat(tileMaxDepth);
    }

    minDepth = clamp(minDepth, 0.0f, 1.0f);
    maxDepth = clamp(maxDepth, 0.0f, 1.0f);

    /* Don't bother doing anything for background (e.g. sky) tiles. */
    if (minDepth < 1.0f)
    {
        /* Calculate tile frustum planes. */
        Frustum frustum;
        CalculateTileFrustum(tileID, minDepth, maxDepth, frustum);

        /* Iterate over lights. Each thread processes a single light. */
        uint lightCount     = constants.lightCount;
        uint lightLoopCount = (lightCount + (threadCount - 1)) / threadCount;

        for (uint i = 0; i < lightLoopCount; i++)
        {
            uint lightIndex = (i * threadCount) + threadIndex;

            /* Since we rounded up the light count to a multiple of the group size,
            * threads in the last iteration may be outside the count. */
            if (lightIndex < lightCount)
            {
                if (IsLightVisible(frustum, lightIndex))
                {
                    uint outputIndex;
                    InterlockedAdd(tileLightCount, 1, outputIndex);
                    tileLightIndices[outputIndex] = lightIndex;
                }
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

    uint elementCount     = (tileLightCount + 2) / 3;
    uint elementLoopCount = (elementCount + (threadCount - 1)) / threadCount;
    uint elementOffset    = tileIndex * kDeferredVisibleLightsTileEntryCount;

    for (uint i = 0; i < elementLoopCount; i++)
    {
        uint elementIndex = (i * threadCount) + threadIndex;

        if (elementIndex < elementCount)
        {
            /* Last 2 indices can go out of bounds of the light array. */
            uint lightOffset = elementIndex * 3;
            uint light0      = tileLightIndices[lightOffset];
            uint light1      = ((lightOffset + 1) < tileLightCount) ? tileLightIndices[lightOffset + 1] : 0;
            uint light2      = ((lightOffset + 2) < tileLightCount) ? tileLightIndices[lightOffset + 2] : 0;

            visibleLights[elementOffset + elementIndex] = (light2 << 20) | (light1 << 10) | light0;
        }
    }
}
