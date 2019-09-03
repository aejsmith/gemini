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

#include "Render/RenderView.h"

#include "GPU/GPUConstantPool.h"
#include "GPU/GPUDevice.h"

RenderView RenderView::CreatePerspective(const glm::vec3&  inPosition,
                                         const glm::quat&  inOrientation,
                                         const float       inVerticalFOV,
                                         const float       inZNear,
                                         const float       inZFar,
                                         const glm::ivec2& inTargetSize)
{
    RenderView view;

    view.mPosition    = inPosition;
    view.mOrientation = inOrientation;
    view.mTargetSize  = inTargetSize;

    /* Calculate the view matrix. This is a world-to-view transformation, so we
     * want the inverse of the given position and orientation. */
    const glm::mat4 positionMatrix    = glm::translate(glm::mat4(), -inPosition);
    const glm::mat4 orientationMatrix = glm::mat4_cast(glm::inverse(inOrientation));
    view.mViewMatrix                  = orientationMatrix * positionMatrix;

    const float aspect = static_cast<float>(inTargetSize.x) / static_cast<float>(inTargetSize.y);
    view.mProjectionMatrix = glm::perspective(glm::radians(inVerticalFOV),
                                              aspect,
                                              inZNear,
                                              inZFar);

    view.mViewProjectionMatrix        = view.mProjectionMatrix * view.mViewMatrix;
    view.mInverseViewProjectionMatrix = glm::inverse(view.mViewProjectionMatrix);

    view.mFrustum = Frustum(view.mViewProjectionMatrix, view.mInverseViewProjectionMatrix);

    view.CreateConstants();
    return view;
}

void RenderView::CreateConstants()
{
    ViewConstants constants;
    constants.viewMatrix                  = mViewMatrix;
    constants.projectionMatrix            = mProjectionMatrix;
    constants.viewProjectionMatrix        = mViewProjectionMatrix;
    constants.inverseViewProjectionMatrix = mInverseViewProjectionMatrix;
    constants.position                    = mPosition;
    constants.targetSize                  = mTargetSize;

    mConstants = GPUDevice::Get().GetConstantPool().Write(&constants, sizeof(constants));
}
