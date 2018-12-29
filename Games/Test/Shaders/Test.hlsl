/*
 * Copyright (C) 2018 Alex Smith
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

struct VSInput
{
    float2      position    : POSITION;
    uint        vertexID    : SV_VERTEXID;
};

struct PSInput
{
    float4      position    : SV_POSITION;
    float4      colour      : COLOR;
};

cbuffer Constants : register(b0, space0)
{
    float4      colours[3];
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.colour   = colours[input.vertexID % 3];
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.colour;
}
