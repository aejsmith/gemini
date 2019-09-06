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

#include "ShaderDefs.h"

struct VSInput
{
    float3      position    : POSITION;
    float2      uv          : TEXCOORD;
};

struct PSInput
{
    float4      position    : SV_POSITION;
    float2      uv          : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = EntityPositionToClip(input.position);
    output.uv       = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return MaterialSample(texture, input.uv);
}
