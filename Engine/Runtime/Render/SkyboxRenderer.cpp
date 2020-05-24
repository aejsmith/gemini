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

#include "Render/SkyboxRenderer.h"

#include "Engine/AssetManager.h"

#include "Render/EntityDrawList.h"
#include "Render/RenderEntity.h"

class SkyboxRenderEntity final : public RenderEntity
{
public:
                                SkyboxRenderEntity(const SkyboxRenderer& renderer);

protected:
    BoundingBox                 GetLocalBoundingBox() override;

    GPUVertexInputStateRef      GetVertexInputState() const override;
    GPUPrimitiveTopology        GetPrimitiveTopology() const override;

    void                        GetGeometry(EntityDrawCall& ioDrawCall) const override;

private:
    const SkyboxRenderer&       GetSkyboxRenderer() const
                                    { return static_cast<const SkyboxRenderer&>(GetRenderer()); }

};

SkyboxRenderEntity::SkyboxRenderEntity(const SkyboxRenderer& renderer) :
    RenderEntity (renderer, *renderer.mMaterial)
{
}

SkyboxRenderer::SkyboxRenderer()
{
    ShaderTechniquePtr technique = AssetManager::Get().Load<ShaderTechnique>("Engine/Techniques/Internal/Skybox");

    mMaterial = new Material(technique);
}

BoundingBox SkyboxRenderEntity::GetLocalBoundingBox()
{
    /* Don't want to cull the skybox. */
    return BoundingBox(glm::vec3(-FLT_MAX), glm::vec3(FLT_MAX));
}

GPUVertexInputStateRef SkyboxRenderEntity::GetVertexInputState() const
{
    /* Skybox is rendered as a fullscreen triangle generated in the shader. */
    return GPUVertexInputState::GetDefault();
}

GPUPrimitiveTopology SkyboxRenderEntity::GetPrimitiveTopology() const
{
    return kGPUPrimitiveTopology_TriangleList;
}

void SkyboxRenderEntity::GetGeometry(EntityDrawCall& ioDrawCall) const
{
    ioDrawCall.vertexCount = 3;
}

SkyboxRenderer::~SkyboxRenderer()
{
}

void SkyboxRenderer::SetTexture(TextureCube* const texture)
{
    mTexture = texture;
    mMaterial->SetArgument("texture", mTexture);

}

EntityRenderer::RenderEntityArray SkyboxRenderer::CreateRenderEntities()
{
    RenderEntityArray entities;
    entities.emplace_back(new SkyboxRenderEntity(*this));
    return entities;
}
