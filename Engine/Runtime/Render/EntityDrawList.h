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

#include "GPU/GPUPipeline.h"

class GPUArgumentSet;
class GPUBuffer;
class GPUGraphicsCommandList;
class RenderGraphPass;

struct CullResults;

/**
 * Structure containing all details for a draw call. This is generated from
 * an entity and stored in an EntityDrawList.
 */
struct EntityDrawCall
{
    struct Buffer
    {
        GPUBuffer*                  buffer = nullptr;
        uint32_t                    offset;
    };

    struct Constants
    {
        uint8_t                     argumentIndex;
        GPUConstants                constants = kGPUConstants_Invalid;
    };

    static constexpr uint32_t       kMaxConstantsPerArgumentSet = 2;

    struct Arguments
    {
        GPUArgumentSet*             argumentSet = nullptr;
        Constants                   constants[kMaxConstantsPerArgumentSet];
    };

    /** Pipeline state. */
    GPUPipelineRef                  pipeline;

    /**
     * Shader arguments. Specifies an argument set to bind at each index, and
     * constants to bind. Null set pointer indicates an unused slot.
     */
    Arguments                       arguments[kMaxArgumentSets];

    /**
     * Vertex buffer bindings. A null buffer pointer indicates an unused slot.
     */
    Buffer                          vertexBuffers[kMaxVertexAttributes];

    /**
     * Index buffer bindings. If the buffer is null, then a non-indexed draw
     * will be used.
     */
    Buffer                          indexBuffer;
    GPUIndexType                    indexType;

    /**
     * Draw parameters. For an indexed draw, vertexOffset gives an offset to
     * add to each index to give the vertex index, while indexOffset gives the
     * offset of the first index to use. For a non-indexed draw, vertexOffset
     * gives the offset of the first vertex to use, and indexOffset is ignored.
     */
    uint32_t                        vertexCount;
    uint32_t                        vertexOffset;
    uint32_t                        indexOffset;
};

/**
 * Key for sorting an entity draw list. Key is just a 64-bit value, and the
 * list will be sorted with keys in ascending order. Different types of pass
 * will want to use different sorting (e.g. opaque incorporates PSO/shader IDs
 * to reduce state changes, while transparent needs to maintain back to front
 * order). There are functions to generate keys for various standard pass
 * types.
 */
class EntityDrawSortKey
{
public:
    uint64_t                        GetValue() const    { return mValue; }

    /** Get a sort key for a standard opaque entity. */
    static EntityDrawSortKey        GetOpaque(const GPUPipelineRef pipeline);

private:
    uint64_t                        mValue;

};

inline bool operator==(const EntityDrawSortKey& a, const EntityDrawSortKey& b)
{
    return a.GetValue() == b.GetValue();
}

inline bool operator<(const EntityDrawSortKey& a, const EntityDrawSortKey& b)
{
    return a.GetValue() < b.GetValue();
}

inline bool operator>(const EntityDrawSortKey& a, const EntityDrawSortKey& b)
{
    return a.GetValue() > b.GetValue();
}

/**
 * List of draw calls with sorting based on key.
 */
class EntityDrawList
{
public:
                                    EntityDrawList();
                                    ~EntityDrawList();

    bool                            IsEmpty() const { return mDrawCalls.empty(); }
    size_t                          Size() const    { return mDrawCalls.size(); }

    /** Allocate space for an expected number of draw calls. */
    void                            Reserve(const size_t expectedCount);

    /**
     * Add an entry to the list. Returns a reference to a draw call structure
     * in the list to be populated (reference may be invalidated by future
     * additions).
     */
    EntityDrawCall&                 Add(const EntityDrawSortKey sortKey);

    /** Sort all entries in the list based on their key. */
    void                            Sort();

    /** Draw the entities to a given command list. */
    void                            Draw(GPUGraphicsCommandList& cmdList) const;

    /**
     * Set the function for a render graph pass to draw the entities. Caller
     * must ensure that the EntityDrawList instance still exists during graph
     * execution.
     */
    void                            Draw(RenderGraphPass& pass) const;

private:
    /**
     * Entry in the draw list. Stores the sort key and the index of the draw
     * call stored in a separate array. Means sorting has to move less data
     * around.
     */
    struct Entry
    {
        EntityDrawSortKey           key;
        size_t                      index;
    };

private:
    std::vector<EntityDrawCall>     mDrawCalls;
    std::vector<Entry>              mEntries;

};
