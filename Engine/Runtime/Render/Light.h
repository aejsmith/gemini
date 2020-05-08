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

#include "Entity/Component.h"

#include "Render/RenderLight.h"

/** Component implementing a light. */
class Light final : public Component
{
    CLASS();

public:
                                Light();

    /** Type of the light. */
    VPROPERTY(LightType, type);
    LightType                   GetType() const { return mType; }
    void                        SetType(const LightType type);

    /** RGB colour of the light (in linear space). */
    VPROPERTY(glm::vec3, colour);
    const glm::vec3&            GetColour() const { return mColour; }
    void                        SetColour(const glm::vec3& colour);

    /**
     * Intensity of the light. Units depend on the type of light:
     *  - Point and spot lights use luminous intensity in candela (lumens per
     *    square radian).
     *  - Directional lights use illuminance in lux.
     */
    VPROPERTY(float, intensity);
    float                       GetIntensity() const { return mIntensity; }
    void                        SetIntensity(const float intensity);

    /**
     * Range of the light. Only relevant for point and spot lights. Light will
     * attenuate over this distance. Must be >= 0. If set to 0, the light is
     * considered to have infinite range.
     */
    VPROPERTY(float, range);
    float                       GetRange() const { return mRange; }
    void                        SetRange(const float range);

    /**
     * Inner cone angle for a spot light. Specifies the angle (in degrees) from
     * the centre at which light begins to fall off. Will be clamped to be >= 0
     * and <= outerConeAngle.
     */
    VPROPERTY(Degrees, innerConeAngle);
    Degrees                     GetInnerConeAngle() const { return mInnerConeAngle; }
    void                        SetInnerConeAngle(const Degrees innerConeAngle);

    /**
     * Outer cone angle for a spot light. Specifies the angle (in degrees) from
     * the centre at which light completely falls off. Will be clamped to be
     * >= innerConeAngle and <= 90.
     */
    VPROPERTY(Degrees, outerConeAngle);
    Degrees                     GetOuterConeAngle() const { return mOuterConeAngle; }
    void                        SetOuterConeAngle(const Degrees outerConeAngle);

    /** Whether the light casts shadows. */
    VPROPERTY(bool, castShadows);
    bool                        GetCastShadows() const { return mCastShadows; }
    void                        SetCastShadows(const bool castShadows);

protected:
    void                        Activated() override;
    void                        Deactivated() override;
    void                        Transformed() override;

private:
                                ~Light();

private:
    LightType                   mType;
    glm::vec3                   mColour;
    float                       mIntensity;
    float                       mRange;
    Degrees                     mInnerConeAngle;
    Degrees                     mOuterConeAngle;
    bool                        mCastShadows;

    RenderLight                 mRenderLight;

};