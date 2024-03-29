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

#include "Render/EntityRenderer.h"
#include "Render/Material.h"

#include "Engine/Mesh.h"

/**
 * Renderer component that renders a Mesh.
 */
class MeshRenderer final : public EntityRenderer
{
    CLASS();

public:
                                    MeshRenderer();

    /** Mesh that will be rendered. */
    VPROPERTY(MeshPtr, mesh);
    void                            SetMesh(Mesh* const mesh);
    Mesh*                           GetMesh() const { return mMesh; }

    /** Materials for the mesh. */
    Material*                       GetMaterial(const uint32_t index) const;
    Material*                       GetMaterial(const std::string& name) const;
    void                            SetMaterial(const uint32_t  index,
                                                Material* const material);
    void                            SetMaterial(const std::string& name,
                                                Material* const    material);

protected:
                                    ~MeshRenderer();

    void                            Serialise(Serialiser& serialiser) const override;
    void                            Deserialise(Serialiser& serialiser) override;

    void                            CustomDebugUIEditor(const uint32_t        flags,
                                                        std::vector<Object*>& ioChildren) override;

    RenderEntityArray               CreateRenderEntities() override;

private:
    MeshPtr                         mMesh;
    std::vector<MaterialPtr>        mMaterials;

};

inline Material* MeshRenderer::GetMaterial(const uint32_t index) const
{
    Assert(index < mMaterials.size());
    return mMaterials[index];
}
