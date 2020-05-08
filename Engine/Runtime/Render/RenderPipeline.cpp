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

#include "Render/RenderPipeline.h"

#include "Render/RenderLight.h"
#include "Render/RenderView.h"

RenderPipeline::RenderPipeline()
{
}

RenderPipeline::~RenderPipeline()
{
}

RenderResourceHandle RenderPipeline::CreateShadowMap(RenderGraph&    graph,
                                                     const LightType lightType,
                                                     const uint16_t  resolution) const
{
    RenderTextureDesc desc;
    desc.name   = "ShadowMap";
    desc.type   = kGPUResourceType_Texture2D;
    desc.format = kShadowMapFormat;
    desc.width  = resolution;
    desc.height = resolution;

    // TODO: Point/directional.
    Assert(lightType == kLightType_Spot);

    return graph.CreateTexture(desc);
}

void RenderPipeline::CreateShadowView(const RenderLight* const light,
                                      const uint16_t           resolution,
                                      RenderView&              outView) const
{
    switch (light->GetType())
    {
        case kLightType_Spot:
            outView = RenderView::CreatePerspective(light->GetPosition(),
                                                    glm::quatLookAt(light->GetDirection(), glm::vec3(0.0f, 1.0f, 0.0f)),
                                                    light->GetConeAngle() * 2,
                                                    0.1f,
                                                    light->GetRange(),
                                                    glm::uvec2(resolution, resolution));

            break;

        default:
            Fatal("TODO: Point/Directional shadows");

    }
}
