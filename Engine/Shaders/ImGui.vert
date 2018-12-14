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

layout (location = 0) in vec2 attribPosition;
layout (location = 1) in vec2 attribUV;
layout (location = 2) in vec4 attribColour;

layout (location = 0) out vec2 vtxUV;
layout (location = 1) out vec4 vtxColour;

layout (set = 0, binding = 2) uniform Uniforms
{
    mat4 projectionMatrix;
};

void main()
{
    vtxUV     = attribUV;
    vtxColour = attribColour;

    gl_Position = projectionMatrix * vec4(attribPosition, 0.0, 1.0);
}
