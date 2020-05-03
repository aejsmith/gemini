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

#include "Core/IntrusiveList.h"
#include "Core/Math/BoundingBox.h"
#include "Core/Math/Transform.h"

#include "GPU/GPUState.h"

#include "Render/RenderDefs.h"

class EntityRenderer;
class GPUPipeline;
class Material;
class RenderContext;

struct EntityDrawCall;

/**
 * This class is the base for a renderable entity in the world. EntityRenderer
 * components attached to world entities (Entity) add one or more renderable
 * entities (RenderEntity) to the RenderWorld.  There is not necessarily a 1:1
 * mapping between world entities and renderable entities, for example a
 * MeshRenderer has a RenderEntity per-sub-mesh.
 */
class RenderEntity
{
public:
    virtual                         ~RenderEntity();

    const EntityRenderer&           GetRenderer() const         { return mRenderer; }

    void                            CreatePipelines();

    void                            SetTransform(const Transform& transform);
    const Transform&                GetTransform() const        { return mTransform; }

    const BoundingBox&              GetWorldBoundingBox() const { return mWorldBoundingBox; }

    /** Return whether this entity supports the specified pass type. */
    bool                            SupportsPassType(const ShaderPassType passType) const
                                        { return mPipelines[passType] != nullptr; }

    /** Get the pipeline for a pass type. */
    GPUPipeline*                    GetPipeline(const ShaderPassType passType) const
                                        { return mPipelines[passType]; }

    /**
     * Populate a draw call structure for the entity in the given pass type.
     * Pass type must be supported (SupportsPassType()).
     */
    void                            GetDrawCall(const ShaderPassType passType,
                                                const RenderContext& context,
                                                EntityDrawCall&      outDrawCall) const;

protected:
                                    RenderEntity(const EntityRenderer& renderer,
                                                 Material&             material);

    /**
     * Get the entity-local bounding box, will be transformed by the entity
     * transform to produce a world bounding box.
     */
    virtual BoundingBox             GetLocalBoundingBox() = 0;

    /**
     * Get vertex input details for the entity. Called from CreatePipelines()
     * at entity activation time to pre-build the pipelines for the entity.
     */
    virtual GPUVertexInputStateRef  GetVertexInputState() const = 0;
    virtual GPUPrimitiveTopology    GetPrimitiveTopology() const = 0;

    /** Populate geometry details in a draw call. */
    virtual void                    GetGeometry(EntityDrawCall& ioDrawCall) const = 0;

private:
    const EntityRenderer&           mRenderer;

    /**
     * Material, not refcounting since owning EntityRenderer is expected to
     * hold a reference.
     */
    Material&                       mMaterial;

    Transform                       mTransform;
    BoundingBox                     mWorldBoundingBox;

    /**
     * Pipelines for each pass type supported by the material's shader
     * technique.
     */
    GPUPipeline*                    mPipelines[kShaderPassTypeCount];

public:
    IntrusiveListNode               mWorldListNode;

};
