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

#ifndef SHADERS_DEFERRED_H
#define SHADERS_DEFERRED_H

#include "DeferredDefs.h"
#include "Lighting.h"

/** Outputs for a G-Buffer pass pixel shader. */
struct DeferredPSOutput
{
    float4  target0 : SV_Target0;
    float4  target1 : SV_Target1;
    float4  target2 : SV_Target2;
    float4  target3 : SV_Target3;
};

/**
 * Encode G-Buffer outputs for the given standard shading model material
 * parameters and surface normal.
 */
DeferredPSOutput EncodeGBuffer(const MaterialParams material,
                               const float3         normal)
{
    DeferredPSOutput output;

    output.target0.rgb = material.baseColour;
    output.target0.a   = 0.0f;

    /* Normal target is unsigned normalized, scale to [0, 1]. */
    output.target1.rgb = (normal + 1.0f) / 2.0f;
    output.target1.a   = 0.0f;

    output.target2.r   = material.metallic;
    output.target2.g   = material.roughness;
    output.target2.b   = material.occlusion;
    output.target2.a   = 0.0f;

    output.target3.rgb = material.emissive;
    output.target3.a   = 0.0f;

    return output;
}

#endif /* SHADERS_DEFERRED_H */
