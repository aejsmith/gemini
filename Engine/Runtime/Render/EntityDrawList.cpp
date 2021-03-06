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

#include "Render/EntityDrawList.h"

#include "GPU/GPUCommandList.h"
#include "GPU/GPUPipeline.h"

#include "Render/RenderGraph.h"

EntityDrawSortKey EntityDrawSortKey::GetOpaque(const GPUPipelineRef pipeline)
{
    /*
     * Currently we have:
     *
     *   | Unused        | PS ID         | VS ID         | Pipeline ID   |
     *   64              48              32              16              0
     *
     * This groups draws using the same shaders together and then by PSO within
     * that to minimise state changes.
     *
     * TODO: Factor distance from camera into this, e.g. by grouping into
     * depth buckets.
     */
    static constexpr uint64_t kPipelineIDShift     = 0;
    static constexpr uint64_t kVertexShaderIDShift = 16;
    static constexpr uint64_t kPixelShaderIDShift  = 32;

    EntityDrawSortKey key = {};

    key.mValue |= static_cast<uint64_t>(pipeline->GetID())                             << kPipelineIDShift;
    key.mValue |= static_cast<uint64_t>(pipeline->GetShaderID(kGPUShaderStage_Vertex)) << kVertexShaderIDShift;
    key.mValue |= static_cast<uint64_t>(pipeline->GetShaderID(kGPUShaderStage_Pixel))  << kPixelShaderIDShift;

    return key;
}

EntityDrawList::EntityDrawList()
{
}

EntityDrawList::~EntityDrawList()
{
}

void EntityDrawList::Reserve(const size_t expectedCount)
{
    mDrawCalls.reserve(expectedCount);
    mEntries.reserve(expectedCount);
}

EntityDrawCall& EntityDrawList::Add(const EntityDrawSortKey sortKey)
{
    Assert(mDrawCalls.size() == mEntries.size());

    Entry& entry = mEntries.emplace_back();
    entry.index  = mDrawCalls.size();
    entry.key    = sortKey;

    return mDrawCalls.emplace_back();
}

void EntityDrawList::Sort()
{
    std::sort(
        mEntries.begin(),
        mEntries.end(),
        [] (const Entry& a, const Entry& b) -> bool
        {
            return a.key < b.key;
        });
}

void EntityDrawList::Draw(GPUGraphicsCommandList& cmdList) const
{
    // TODO: Implement draw parallelisation here. Partition the list into jobs
    // and execute in parallel, combine command lists in order at the end.
    // Would need handling for certain bits of state that we don't override
    // from the EntityDrawCall (viewport/scissor).
    //
    // An additional thing that we could do is add some support in the render
    // graph for this, so for example we can tell the graph that it needs to
    // wait for a given set of jobs to complete and submit them into the
    // command list for the pass, before submitting that to the context. That
    // would let us return out of here without waiting for completion of all
    // the jobs, and continue execution of subsequent passes on this thread.

    for (const Entry& entry : mEntries)
    {
        const EntityDrawCall& drawCall = mDrawCalls[entry.index];

        /*
         * The GPU layer is responsible for avoiding redundant state changes
         * so we'll just pass everything through.
         */

        cmdList.SetPipeline(drawCall.pipeline);

        for (size_t i = 0; i < ArraySize(drawCall.arguments); i++)
        {
            const EntityDrawCall::Arguments& arguments = drawCall.arguments[i];

            if (arguments.argumentSet)
            {
                cmdList.SetArguments(i, arguments.argumentSet);

                for (auto& constants : arguments.constants)
                {
                    if (constants.constants != kGPUConstants_Invalid)
                    {
                        cmdList.SetConstants(i, constants.argumentIndex, constants.constants);
                    }
                }
            }
        }

        for (size_t i = 0; i < ArraySize(drawCall.vertexBuffers); i++)
        {
            const EntityDrawCall::Buffer& vertexBuffer = drawCall.vertexBuffers[i];

            if (vertexBuffer.buffer)
            {
                cmdList.SetVertexBuffer(i, vertexBuffer.buffer, vertexBuffer.offset);
            }
        }

        const EntityDrawCall::Buffer& indexBuffer = drawCall.indexBuffer;

        if (indexBuffer.buffer)
        {
            cmdList.SetIndexBuffer(drawCall.indexType, indexBuffer.buffer, indexBuffer.offset);

            cmdList.DrawIndexed(drawCall.vertexCount, drawCall.indexOffset, drawCall.vertexOffset);
        }
        else
        {
            cmdList.Draw(drawCall.vertexCount, drawCall.vertexOffset);
        }
    }
}

void EntityDrawList::Draw(RenderGraphPass& pass) const
{
    pass.SetFunction([this] (const RenderGraph&      graph,
                             const RenderGraphPass&  pass,
                             GPUGraphicsCommandList& cmdList)
    {
        Draw(cmdList);
    });
}
