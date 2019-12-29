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

#ifndef SHADERS_LIGHTING_H
#define SHADERS_LIGHTING_H

#include "LightingDefs.h"

/**
 * References:
 *  [1] glTF Version 2.0, Appendix B: BRDF Implementation
 *      https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#appendix-b-brdf-implementation
 *  [2] glTF Version 2.0, KHR_lights_punctual
 *      https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md
 *  [3] Physically Based Shading at Disney
 *      https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
 *  [4] Real Shading in Unreal Engine 4
 *      https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
 *  [5] An Inexpensive BRDF Model for Physically-based Rendering
 *      https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
 *  [6] A Reflectance Model for Computer Graphics
 *      http://inst.cs.berkeley.edu/~cs294-13/fa09/lectures/cookpaper.pdf
 */

/** Input material parameters using the metallic-roughness model. */
struct MaterialParams
{
    float4      baseColour;
    float       metallic;
    float       roughness;
    float3      emissive;
    float       occlusion;
};

/** Parameters to the BRDF derived from material parameters. */
struct BRDFParams
{
    float3      diffuseColour;
    float3      f0;
    float       alphaRoughness;
    float       alphaRoughnessSq;
};

/** Parameters describing the surface/point being shaded. */
struct SurfaceParams
{
    /** World space position. */
    float3      position;

    /** World space normal vector. */
    float3      N;

    /** Normalized direction from surface to the view. */
    float3      V;

    float       NdotV;
};

/**
 * Perturb an interpolated vertex normal based on a value sampled from a
 * normal map. Sampled normal is expected to be in the [0,1] range, will be
 * scaled by this function to [-1, 1]. Vertex normal need not be normalized,
 * will be done by this function.
 *
 * Uses the method described by http://www.thetenthplanet.de/archives/1180 to
 * do this without pre-computed tangent/bitangent vectors.
 */
float3 PerturbNormal(const float3 vertexNormal,
                     const float3 sampledNormal,
                     const float3 worldPosition,
                     const float2 uv)
{
    float3 dxPos = ddx(worldPosition);
    float3 dyPos = ddy(worldPosition);
    float2 dxUV  = ddx(uv);
    float2 dyUV  = ddy(uv);

    float3 N = normalize(vertexNormal);
    float3 T = ((dyUV.y * dxPos) - (dxUV.y * dyPos)) / ((dxUV.x * dyUV.y) - (dyUV.x * dxUV.y));
           T = normalize(T - (N * dot(N, T)));
    float3 B = normalize(cross(N, T));

    float3 tangentNormal = (sampledNormal * 2.0f) - 1.0f;
    return normalize(mul(tangentNormal, float3x3(T, B, N)));
}

/** Convert material parameters to BRDF parameters. */
BRDFParams CalculateBRDFParams(const MaterialParams material)
{
    const float roughness = saturate(material.roughness);
    const float metallic  = saturate(material.metallic);

    BRDFParams params;

    /* Materials are authored with perceptual roughness, square to give alpha
     * parameter for BRDF. See page 15 of [3]. */
    params.alphaRoughness   = roughness * roughness;
    params.alphaRoughnessSq = params.alphaRoughness * params.alphaRoughness;

    /*
     * Metallic factor specifies whether the material is dielectric or metal.
     *
     * If it is 0, the material is fully dielectric:
     *  - F0 = Fixed 4% for the metallic-roughness model.
     *  - Diffuse colour = Base colour * 96% (energy conserving, 4% reflected
     *    so have to scale base colour).
     *
     * If it is 1, the material is fully metal:
     *  - F0 = Base colour (metals tend to reflect most light and the
     *    reflections can be tinted e.g. gold/brass).
     *  - Diffuse colour = Black (any light not reflected is absorbed rather
     *    than scattered).
     *
     * The metallic factor is not a binary 0 or 1, we can have partially
     * metallic surfaces, e.g. to blend between rusty and clear parts of a
     * metal surface. Therefore, we use the factor to lerp between the fully
     * dielectric and fully metal states.
     */
    const float3 dielectricF0 = float3(0.04f, 0.04f, 0.04f);
    params.diffuseColour      = lerp(material.baseColour.rgb * (1 - dielectricF0.r), float3(0.0f, 0.0f, 0.0f), metallic);
    params.f0                 = lerp(dielectricF0, material.baseColour.rgb, metallic);

    return params;
}

SurfaceParams CalculateSurfaceParams(const float3 position,
                                     const float3 normal)
{
    SurfaceParams params;

    params.position = position;
    params.N        = normal;
    params.V        = normalize(view.position - position);
    params.NdotV    = saturate(dot(params.N, params.V));

    return params;
}

/** Lambertian diffuse contribution. */
float3 LambertDiffuse(const BRDFParams material)
{
    return material.diffuseColour / SHADER_PI;
}

/** Trowbridge-Reitz/GGX normal distribution function (D). */
float D_TrowbridgeReitzGGX(const BRDFParams material,
                           const float3     N,
                           const float3     H)
{
    const float NdotH = saturate(dot(N, H));
    const float D     = (((NdotH * material.alphaRoughnessSq) - NdotH) * NdotH) + 1.0;
    return material.alphaRoughnessSq / (SHADER_PI * D * D);
}

/** Fresnel-Schlick approximation (F). */
float3 F_Schlick(const BRDFParams material,
                 const float3     V,
                 const float3     H)
{
    const float VdotH = saturate(dot(V, H));
    return material.f0 + (1.0f - material.f0) * pow(1.0f - VdotH, 5.0f);
}

/**
 * Smith Joint GGX geometric occlusion function.
 * Vis = G / (4 * (N.L) * (N.V))
 */
float Vis_SmithJointGGX(const BRDFParams material,
                      const float      NdotL,
                      const float      NdotV)
{
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0f - material.alphaRoughnessSq) + material.alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0f - material.alphaRoughnessSq) + material.alphaRoughnessSq);
    float GGX  = GGXV + GGXL;

    return (GGX > 0.0f) ? 0.5f / (GGXV + GGXL) : 0.0f;
}

float3 CookTorranceBRDF(const LightParams   light,
                        const BRDFParams    material,
                        const SurfaceParams surface,
                        const float3        toLight)
{
    const float3 N = surface.N;
    const float3 V = surface.V;
    const float3 L = normalize(toLight);
    const float3 H = normalize(L + V);

    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));

    float3 result;

    if (NdotL > 0.0f || NdotV > 0.0f)
    {
        const float  D   = D_TrowbridgeReitzGGX(material, N, H);
        const float3 F   = F_Schlick(material, V, H);
        const float  Vis = Vis_SmithJointGGX(material, NdotL, NdotV);

        const float3 diffuse  = (1.0f - F) * LambertDiffuse(material);
        const float3 specular = F * D * Vis;

        result = NdotL * (diffuse + specular);
    }
    else
    {
        result = float3(0, 0, 0);
    }

    return result;
}

float CalculateLightAttenuation(const float range,
                                const float distance)
{
    /* https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property */

    if (range <= 0.0f)
    {
        return 1.0f;
    }

    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

float3 CalculatePointLight(const LightParams   light,
                           const BRDFParams    material,
                           const SurfaceParams surface)
{
    const float3 toLight    = light.position - surface.position;
    const float distance    = length(toLight);
    const float attenuation = CalculateLightAttenuation(light.range, distance);
    const float3 brdfResult = CookTorranceBRDF(light, material, surface, toLight);

    return attenuation * light.intensity * light.colour * brdfResult;
}

float3 CalculateLight(const LightParams   light,
                      const BRDFParams    material,
                      const SurfaceParams surface)
{
    if (light.type == kShaderLightType_Point)
    {
        return CalculatePointLight(light, material, surface);
    }
    else
    {
        // TODO: Spot/directional
        return float3(0.0f, 0.0f, 0.0f);
    }
}

#endif /* SHADERS_LIGHTING_H */
