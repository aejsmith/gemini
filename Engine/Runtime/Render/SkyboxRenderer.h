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

#include "Engine/Texture.h"

#include "Render/EntityRenderer.h"
#include "Render/Material.h"

/**
 * This component renders a skybox - a cube texture rendered onto the far plane,
 * behind everything else. Entity transformation has no effect on the skybox.
 */
class SkyboxRenderer final : public EntityRenderer
{
    CLASS();

public:
                                SkyboxRenderer();

    /** Texture to use for the skybox. If null, the colour will be used. */
    VPROPERTY(TextureCubePtr, texture);
    TextureCube*                GetTexture() const  { return mTexture; }
    void                        SetTexture(TextureCube* const texture);

    /** Solid colour to use when no texture is specified. */
    VPROPERTY(glm::vec3, colour);
    const glm::vec3&            GetColour() const   { return mColour; }
    void                        SetColour(const glm::vec3& colour);

protected:
                                ~SkyboxRenderer();

    RenderEntityArray           CreateRenderEntities() override;

private:
    void                        CreateMaterial();

private:
    TextureCubePtr              mTexture;
    glm::vec3                   mColour;

    MaterialPtr                 mMaterial;

    friend class SkyboxRenderEntity;
};
