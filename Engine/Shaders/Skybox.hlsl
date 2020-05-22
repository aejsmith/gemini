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

#include "ShaderDefs.h"

struct PSInput
{
    float4 position : SV_Position;
    float3 viewDirection;
};

PSInput VSMain(const uint vertexID : SV_VertexID)
{
    PSInput output;

    /* Output a fullscreen triangle covering the far plane. */
    output.position.x = ((float)(vertexID % 2) * 4.0f) - 1.0f;
    output.position.y = ((float)(vertexID / 2) * 4.0f) - 1.0f;
    output.position.z = 1.0f;
    output.position.w = 1.0f;

    /* Calculate the viewing direction into the skybox (interpolated into the
     * pixel shader). We want to cancel out the translation of the matrix so
     * subtract the position. */
    float4 direction = mul(view.inverseViewProjection, output.position);
    output.viewDirection = (direction.xyz / direction.w) - view.position;

    return output;
}

float4 PSMain(PSInput input) : SV_Target0
{
    return MaterialSample(texture, input.viewDirection);
}
