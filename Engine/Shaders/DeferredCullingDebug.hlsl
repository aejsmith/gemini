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
#include "FullScreen.h"

StructuredBuffer<uint> visibleLightCount : SRV(DeferredCullingDebug, VisibleLightCount);

CBUFFER(DeferredCullingDebugConstants, constants, DeferredCullingDebug, Constants);

float3 Temperature(uint count, uint maximum)
{
    const float3 red   = float3(1.0f, 0.0f, 0.0f);
    const float3 green = float3(0.0f, 1.0f, 0.0f);
    const float3 blue  = float3(0.0f, 0.0f, 1.0f);

    float fraction = min(float(count) / float(maximum), 1.0f);

    if (fraction < 0.5f)
    {
        return lerp(blue, green, fraction * 2);
    }
    else
    {
        return lerp(green, red, (fraction - 0.5f) * 2);
    }
}

float4 PSMain(FullScreenPSInput input) : SV_TARGET
{
    uint2 tileID   = input.position / kDeferredTileSize;
    uint tileIndex = (tileID.y * constants.tileDimensions.x) + tileID.x;

    uint tileLightCount = visibleLightCount.Load(tileIndex);

    if (tileLightCount == 0)
    {
        discard;
    }

    return float4(Temperature(tileLightCount, constants.maxLightCount), 0.5f);
}
