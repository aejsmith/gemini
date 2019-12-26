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

#include "FullScreen.h"
#include "Tonemap.h"

Texture2D sourceTexture : TEXTURE_REGISTER(kArgumentSet_Tonemap, kTonemapArguments_SourceTexture);

float3 ToneMapUnchartedImpl(float3 x)
{
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;

    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

float3 ToneMapUncharted(float3 colour)
{
    const float whitePoint   = 11.2f;
    const float exposureBias = 2.0f;

    float3 c          = ToneMapUnchartedImpl(colour * exposureBias);
    float3 whiteScale = ToneMapUnchartedImpl(float3(whitePoint, whitePoint, whitePoint));

    return c / whiteScale;
}

float4 PSMain(FullScreenPSInput input) : SV_TARGET
{
    float4 result = sourceTexture.Load(input.position);

    /* Tonemap. */
    result.xyz = ToneMapUncharted(result.xyz);

    /* Convert to gamma space. */
    result.xyz = pow(result.xyz, 1.0f / 2.2f);

    return result;
}
