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

#pragma once

#include "Core/Math.h"

/**
 * This class encapsulates a 3D object transformation (position, orientation
 * and scale), and calculates transformation matrices.
 */
class Transform
{
public:
                            Transform();
                            Transform(const glm::vec3& inPosition,
                                      const glm::quat& inOrientation,
                                      const glm::vec3& inScale);

    /**
     * Update the whole transformation. This should be preferred when changing
     * multiple parts of the transformation, since it allows the matrix update
     * to be performed once for the multiple updates, compared to using the
     * individual setters which update the matrix each call.
     */
    void                    Set(const glm::vec3& inPosition,
                                const glm::quat& inOrientation,
                                const glm::vec3& inScale);

    const glm::vec3&        GetPosition() const { return mPosition; }
    void                    SetPosition(const glm::vec3& inPosition);

    const glm::quat&        GetOrientation() const { return mOrientation; }
    void                    SetOrientation(const glm::quat& inOrientation);

    const glm::vec3&        GetScale() const { return mScale; }
    void                    SetScale(const glm::vec3& inScale);

    const glm::mat4&        GetMatrix() const { return mMatrix; }

    /** Get the inverse transformation matrix. Recalculated on each call. */
    glm::mat4               CalculateInverseMatrix() const;

private:
    void                    UpdateMatrix();

private:
    glm::vec3               mPosition;
    glm::quat               mOrientation;
    glm::vec3               mScale;
    glm::mat4               mMatrix;

};

inline Transform::Transform() :
    mPosition       (0.0f),
    mOrientation    (1.0f, 0.0f, 0.0f, 0.0f),
    mScale          (1.0f),
    mMatrix         (1.0f)
{
}

inline Transform::Transform(const glm::vec3& inPosition,
                            const glm::quat& inOrientation,
                            const glm::vec3& inScale) :
    mPosition       (inPosition),
    mOrientation    (inOrientation),
    mScale          (inScale)
{
    UpdateMatrix();
}

inline void Transform::Set(const glm::vec3& inPosition,
                           const glm::quat& inOrientation,
                           const glm::vec3& inScale)
{
    mPosition    = inPosition;
    mOrientation = inOrientation;
    mScale       = inScale;

    UpdateMatrix();
}

inline void Transform::SetPosition(const glm::vec3& inPosition)
{
    mPosition = inPosition;

    UpdateMatrix();
}

inline void Transform::SetOrientation(const glm::quat& inOrientation)
{
    mOrientation = inOrientation;

    UpdateMatrix();
}

inline void Transform::SetScale(const glm::vec3& inScale)
{
    mScale = inScale;

    UpdateMatrix();
}

inline void Transform::UpdateMatrix()
{
    mMatrix = glm::translate(glm::mat4(), mPosition) *
              glm::mat4_cast(mOrientation) *
              glm::scale(glm::mat4(), mScale);
}

inline glm::mat4 Transform::CalculateInverseMatrix() const
{
    return glm::affineInverse(mMatrix);
}
