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

#include "ImGUI.h"

struct VSInput
{
    float2      position    : POSITION;
    float2      uv          : TEXCOORD;
    float4      colour      : COLOR;
};

struct PSInput
{
    float4      position    : SV_Position;
    float2      uv          : TEXCOORD;
    float4      colour      : COLOR;
};

Texture2D fontTexture    : SRV(ImGUI, FontTexture);
SamplerState fontSampler : SMP(ImGUI, FontSampler);

CBUFFER(ImGUIConstants, constants, ImGUI, Constants);

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(constants.projectionMatrix, float4(input.position, 0.0, 1.0));
    output.uv       = input.uv;
    output.colour   = input.colour;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.colour * fontTexture.Sample(fontSampler, input.uv);
}
