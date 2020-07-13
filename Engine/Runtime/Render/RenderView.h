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

#pragma once

#include "Render/RenderDefs.h"

#include "Core/Math/Frustum.h"

/**
 * This class specifies viewing parameters (position/orientation, projection,
 * etc.) for rendering a world. It is immutable once created.
 */
class RenderView
{
public:
                            RenderView() {}

    /** Create a view with a perspective projection. */
    static RenderView       CreatePerspective(const glm::vec3&  position,
                                              const glm::quat&  orientation,
                                              const Radians     verticalFOV,
                                              const float       zNear,
                                              const float       zFar,
                                              const glm::uvec2& targetSize,
                                              const bool        createConstants = true);

    /** Create a view with an orthographic projection. */
    static RenderView       CreateOrthographic(const glm::vec3&  position,
                                               const glm::quat&  orientation,
                                               const float       left,
                                               const float       right,
                                               const float       bottom,
                                               const float       top,
                                               const float       zNear,
                                               const float       zFar,
                                               const glm::uvec2& targetSize,
                                               const bool        createConstants = true);

    bool                    IsPerspective() const
                                { return mVerticalFOV != 0.0f; }

    const glm::vec3&        GetPosition() const                     { return mPosition; }
    const glm::quat&        GetOrientation() const                  { return mOrientation; }
    Radians                 GetVerticalFOV() const                  { return mVerticalFOV; }
    float                   GetZNear() const                        { return mZNear; }
    float                   GetZFar() const                         { return mZFar; }
    const glm::uvec2&       GetTargetSize() const                   { return mTargetSize; }
    const glm::mat4&        GetViewMatrix() const                   { return mViewMatrix; }
    const glm::mat4&        GetProjectionMatrix() const             { return mProjectionMatrix; }
    const glm::mat4&        GetViewProjectionMatrix() const         { return mViewProjectionMatrix; }
    const glm::mat4&        GetInverseViewProjectionMatrix() const  { return mInverseViewProjectionMatrix; }
    const Frustum&          GetFrustum() const                      { return mFrustum; }

    GPUConstants            GetConstants() const                    { return mConstants; }

private:
    void                    InitView(const bool createConstants);

private:
    glm::vec3               mPosition;
    glm::quat               mOrientation;
    Radians                 mVerticalFOV;
    float                   mZNear;
    float                   mZFar;
    glm::uvec2              mTargetSize;

    glm::mat4               mViewMatrix;
    glm::mat4               mProjectionMatrix;
    glm::mat4               mViewProjectionMatrix;
    glm::mat4               mInverseViewProjectionMatrix;

    Frustum                 mFrustum;

    GPUConstants            mConstants;

};
