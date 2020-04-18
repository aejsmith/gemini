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

#ifndef SHADERS_DEFERREDDEFS_H
#define SHADERS_DEFERREDDEFS_H

#include "LightingDefs.h"

#define kDeferredTileSize                               16

/**
 * VisibleLights array is an array of uints with 3 10-bit indices packed into
 * each entry.
 */
#define kDeferredMaxLightCount                          1024
#define kDeferredVisibleLightsPerEntry                  3
#define kDeferredVisibleLightsTileEntryCount            342

/**
 * Light culling shader arguments.
 */

/** Culling uses the ViewEntity set at index 0. */
#define kArgumentSet_DeferredCulling                    1

#define kDeferredCullingArguments_DepthTexture          0
#define kDeferredCullingArguments_LightParams           1
#define kDeferredCullingArguments_VisibleLightCount     2
#define kDeferredCullingArguments_VisibleLights         3
#define kDeferredCullingArguments_Constants             4
#define kDeferredCullingArgumentsCount                  5

struct DeferredCullingConstants
{
    shader_uint2    tileDimensions;
    shader_uint     lightCount;
};

#endif /* SHADERS_DEFERREDDEFS_H */
