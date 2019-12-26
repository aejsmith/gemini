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

#ifndef SHADERS_FULLSCREEN_H
#define SHADERS_FULLSCREEN_H

struct FullScreenPSInput
{
    float4      position    : SV_Position;
    float2      uv          : TEXCOORD0;
};

/**
 * Vertex shader which outputs a counter-clockwise full-screen triangle based
 * on SV_VertexID, no vertex input data is required.
 */
FullScreenPSInput VSFullScreen(const uint inVertexID : SV_VertexID)
{
    FullScreenPSInput output;

    output.position.x = ((float)(inVertexID % 2) * 4.0f) - 1.0f;
    output.position.y = ((float)(inVertexID / 2) * 4.0f) - 1.0f;
    output.position.z = 0.0f;
    output.position.w = 1.0f;

    output.uv.x       = (float)(inVertexID % 2) * 2.0f;
    output.uv.y       = 1.0f - ((float)(inVertexID / 2) * 2.0f);

    return output;
}

#endif /* SHADERS_FULLSCREEN_H */
