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

#include "FullScreen.h"
#include "FXAA.h"

Texture2D sourceTexture    : SRV(FXAA, SourceTexture);
SamplerState sourceSampler : SMP(FXAA, SourceSampler);

CBUFFER(FXAAConstants, constants, FXAA, Constants);

float FxaaLuma(float4 rgba)
{
    return dot(rgba.rgb, float3(0.299f, 0.587f, 0.114f));
}

#define FXAA_PC                 1
#define FXAA_HLSL_5             1
#define FXAA_QUALITY__PRESET    12
#define FXAA_CUSTOM_LUMA        1

/* This path does not support FXAA_CUSTOM_LUMA. */
#define FXAA_GATHER4_ALPHA      0

#include "../3rdParty/FXAA/Fxaa3_11.h"

float4 PSMain(FullScreenPSInput input) : SV_Target
{
    FxaaTex tex;
    tex.smpl = sourceSampler;
    tex.tex  = sourceTexture;

    return FxaaPixelShader(
        input.uv,
        float4(0.0f, 0.0f, 0.0f, 0.0f),     // FxaaFloat4 fxaaConsolePosPos
        tex,                                // FxaaTex tex
        tex,                                // FxaaTex fxaaConsole360TexExpBiasNegOne
        tex,                                // FxaaTex fxaaConsole360TexExpBiasNegTwo
        constants.rcpFrame,                 // FxaaFloat2 fxaaQualityRcpFrame
        float4(0.0f, 0.0f, 0.0f, 0.0f),     // FxaaFloat4 fxaaConsoleRcpFrameOpt
        float4(0.0f, 0.0f, 0.0f, 0.0f),     // FxaaFloat4 fxaaConsoleRcpFrameOpt2
        float4(0.0f, 0.0f, 0.0f, 0.0f),     // FxaaFloat4 fxaaConsole360RcpFrameOpt2
        0.75f,                              // FxaaFloat fxaaQualitySubpix
        0.166f,                             // FxaaFloat fxaaQualityEdgeThreshold
        0.0833f,                            // FxaaFloat fxaaQualityEdgeThresholdMin
        0.0f,                               // FxaaFloat fxaaConsoleEdgeSharpness
        0.0f,                               // FxaaFloat fxaaConsoleEdgeThreshold
        0.0f,                               // FxaaFloat fxaaConsoleEdgeThresholdMin
        float4(0.0f, 0.0f, 0.0f, 0.0f)      // FxaaFloat fxaaConsole360ConstDir
    );
}
