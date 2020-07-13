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

#include "Render/RenderView.h"

#include "GPU/GPUConstantPool.h"
#include "GPU/GPUDevice.h"

RenderView RenderView::CreatePerspective(const glm::vec3&  position,
                                         const glm::quat&  orientation,
                                         const Radians     verticalFOV,
                                         const float       zNear,
                                         const float       zFar,
                                         const glm::uvec2& targetSize,
                                         const bool        createConstants)
{
    RenderView view;

    view.mPosition    = position;
    view.mOrientation = orientation;
    view.mVerticalFOV = verticalFOV;
    view.mZNear       = zNear;
    view.mZFar        = zFar;
    view.mTargetSize  = targetSize;

    const float aspect = static_cast<float>(targetSize.x) / static_cast<float>(targetSize.y);
    view.mProjectionMatrix = glm::perspective(verticalFOV, aspect, zNear, zFar);

    view.InitView(createConstants);

    return view;
}

RenderView RenderView::CreateOrthographic(const glm::vec3&  position,
                                          const glm::quat&  orientation,
                                          const float       left,
                                          const float       right,
                                          const float       bottom,
                                          const float       top,
                                          const float       zNear,
                                          const float       zFar,
                                          const glm::uvec2& targetSize,
                                          const bool        createConstants)
{
    RenderView view;

    view.mPosition    = position;
    view.mOrientation = orientation;
    view.mVerticalFOV = 0.0f;
    view.mZNear       = zNear;
    view.mZFar        = zFar;
    view.mTargetSize  = targetSize;

    view.mProjectionMatrix = glm::ortho(left, right, bottom, top, zNear, zFar);

    view.InitView(createConstants);

    return view;
}

void RenderView::InitView(const bool createConstants)
{
    /* Calculate the view matrix. This is a world-to-view transformation, so we
     * want the inverse of the given position and orientation. */
    const glm::mat4 positionMatrix    = glm::translate(glm::mat4(), -mPosition);
    const glm::mat4 orientationMatrix = glm::mat4_cast(glm::inverse(mOrientation));
    mViewMatrix                       = orientationMatrix * positionMatrix;
    mViewProjectionMatrix             = mProjectionMatrix * mViewMatrix;
    mInverseViewProjectionMatrix      = glm::inverse(mViewProjectionMatrix);
    mFrustum                          = Frustum(mViewProjectionMatrix, mInverseViewProjectionMatrix);

    if (createConstants)
    {
        ViewConstants constants;
        constants.view                  = mViewMatrix;
        constants.projection            = mProjectionMatrix;
        constants.viewProjection        = mViewProjectionMatrix;
        constants.inverseView           = glm::inverse(mViewMatrix);
        constants.inverseProjection     = glm::inverse(mProjectionMatrix);
        constants.inverseViewProjection = mInverseViewProjectionMatrix;
        constants.position              = mPosition;
        constants.zNear                 = mZNear;
        constants.zFar                  = mZFar;
        constants.targetSize            = mTargetSize;

        mConstants = GPUDevice::Get().GetConstantPool().Write(&constants, sizeof(constants));
    }
    else
    {
        mConstants = kGPUConstants_Invalid;
    }
}
