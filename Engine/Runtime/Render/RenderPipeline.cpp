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

/*
 * References:
 *  [1] "Cascaded Shadow Maps" (NVIDIA Corporation)
 *      http://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
 */

#include "Render/RenderPipeline.h"

#include "Render/RenderLight.h"
#include "Render/RenderView.h"

RenderPipeline::RenderPipeline() :
    directionalShadowMaxDistance    (50.0f),
    shadowMapResolution             (512),
    mDirectionalShadowCascades      (3),
    mDirectionalShadowSplitFactor   (0.8f)
{
}

RenderPipeline::~RenderPipeline()
{
}

void RenderPipeline::SetDirectionalShadowCascades(const uint8_t cascades)
{
    Assert(cascades >= 1 && cascades <= kMaxShadowCascades);

    mDirectionalShadowCascades = cascades;
}

void RenderPipeline::SetDirectionalShadowSplitFactor(const float factor)
{
    Assert(factor >= 0.0f && factor <= 1.0f);

    mDirectionalShadowSplitFactor = factor;
}

RenderResourceHandle RenderPipeline::CreateShadowMap(RenderGraph&    graph,
                                                     const LightType lightType) const
{
    RenderTextureDesc desc;
    desc.name   = "ShadowMap";
    desc.type   = kGPUResourceType_Texture2D;
    desc.format = kShadowMapFormat;
    desc.width  = this->shadowMapResolution;
    desc.height = this->shadowMapResolution;

    switch (lightType)
    {
        case kLightType_Spot:
            desc.arraySize = 1;
            break;

        case kLightType_Directional:
            desc.arraySize = mDirectionalShadowCascades;
            break;

        case kLightType_Point:
            Fatal("TODO: Point light shadows");
            break;

        default:
            Unreachable();

    }

    return graph.CreateTexture(desc);
}

void RenderPipeline::CreateShadowViews(const RenderLight&       light,
                                       const RenderView&        cameraView,
                                       std::vector<RenderView>& outViews,
                                       float* const             outSplitDepths) const
{
    const glm::uvec2 targetSize = glm::uvec2(this->shadowMapResolution, this->shadowMapResolution);

    switch (light.GetType())
    {
        case kLightType_Spot:
        {
            outViews.resize(1);

            outViews[0] =
                RenderView::CreatePerspective(light.GetPosition(),
                                              glm::quatLookAt(light.GetDirection(), glm::vec3(0.0f, 1.0f, 0.0f)),
                                              light.GetConeAngle() * 2,
                                              0.1f,
                                              light.GetRange(),
                                              targetSize);

            break;
        }

        case kLightType_Directional:
        {
            /* Calculate the cascade splits using the algorithm from [1]. */
            const float zNear = cameraView.GetZNear();
            const float zFar  = std::min(cameraView.GetZFar(), this->directionalShadowMaxDistance);

            for (uint8_t cascade = 0; cascade < mDirectionalShadowCascades; cascade++)
            {
                const float frac        = static_cast<float>(cascade + 1) / mDirectionalShadowCascades;
                const float exp         = zNear * powf(zFar / zNear, frac);
                const float linear      = zNear + (frac * (zFar - zNear));
                outSplitDepths[cascade] = glm::mix(linear, exp, mDirectionalShadowSplitFactor);
            }

            /*
             * Calculate views, again following [1]. For each cascade:
             *
             *  1. Transform the camera view into light space.
             *  2. Trim this view's near/far planes to cover the cascade.
             *  3. Calculate the minimum and maximum from all 8 frustum points,
             *     which forms a bounding box aligned in the light's direction.
             *  4. Use this box to produce an orthographic projection forming
             *     the shadow view.
             *
             * TODO: This currently assumes that the camera view is a
             * perspective projection. This code is also probably not as
             * efficient as it could be, but right now I prefer clarity to
             * performance.
             */
            Assert(cameraView.IsPerspective());

            outViews.resize(mDirectionalShadowCascades);

            const glm::quat toLight             = glm::inverse(light.GetOrientation());
            const glm::vec3 camLightPosition    = toLight * cameraView.GetPosition();
            const glm::quat camLightOrientation = toLight * cameraView.GetOrientation();

            for (uint8_t cascade = 0; cascade < mDirectionalShadowCascades; cascade++)
            {
                const float cascadeNear = (cascade > 0) ? outSplitDepths[cascade - 1] : zNear;
                const float cascadeFar  = outSplitDepths[cascade];

                const RenderView cascadeView =
                    RenderView::CreatePerspective(camLightPosition,
                                                  camLightOrientation,
                                                  cameraView.GetVerticalFOV(),
                                                  cascadeNear,
                                                  cascadeFar,
                                                  cameraView.GetTargetSize(),
                                                  false);

                glm::vec3 minimum( std::numeric_limits<float>::max());
                glm::vec3 maximum(-std::numeric_limits<float>::max());

                for (uint32_t i = 0; i < Frustum::kNumCorners; i++)
                {
                    const glm::vec3 corner = cascadeView.GetFrustum().GetCorner(i);

                    minimum = glm::min(minimum, corner);
                    maximum = glm::max(maximum, corner);
                }

                const glm::vec3 extent     = maximum - minimum;
                const glm::vec3 halfExtent = extent / 2.0f;
                const glm::vec3 centre     = minimum + halfExtent;

                /*
                 * Shadow camera is positioned at the back centre of the
                 * bounding box, looking in (along the camera orientation) -
                 * transformed back into world space since that's what
                 * RenderView wants.
                 */
                const glm::vec3 shadowPosition =
                    light.GetOrientation() * glm::vec3(centre.x, centre.y, maximum.z);

                outViews[cascade] =
                    RenderView::CreateOrthographic(shadowPosition,
                                                   light.GetOrientation(),
                                                   -halfExtent.x,
                                                   halfExtent.x,
                                                   -halfExtent.y,
                                                   halfExtent.y,
                                                   0.0f,
                                                   extent.z,
                                                   targetSize);
            }

            break;
        }

        default:
            Fatal("TODO: Point light shadows");

    }
}
