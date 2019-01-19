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

#include "GPU/GPUState.h"

#include "Core/HashTable.h"

#include <mutex>
#include <shared_mutex>

template <typename D>
struct GPUState<D>::Cache
{
    HashMap<size_t, GPUState<D>*>   cache;

    /**
     * Lock for the cache. This needs to be thread-safe since any thread can
     * look up states at any time. Use a shared mutex to allow simultaneous
     * reads - only need to lock for writing if we need to cache a new state.
     */
    std::shared_mutex               lock;
};

template <typename D>
const GPUState<D>* GPUState<D>::Get(const Desc& inDesc)
{
    const size_t hash = HashValue(inDesc);

    /* Check whether we have a copy of the descriptor stored. Lock for reading
     * to begin with. */
    GPUState* state = nullptr;

    {
        std::shared_lock lock(mCache.lock);

        auto it = mCache.cache.find(hash);
        if (it != mCache.cache.end())
        {
            state = it->second;

            /* Sanity check that we aren't getting any hash collisions. */
            Assert(inDesc == state->GetDesc());
        }
    }

    if (!state)
    {
        std::unique_lock lock(mCache.lock);

        state = new GPUState(inDesc);

        auto ret = mCache.cache.emplace(hash, state);

        if (!ret.second)
        {
            delete state;
            state = ret.first->second;

            Assert(inDesc == state->GetDesc());
        }
    }

    return state;
}

/* Explicit instantiations. */
template<> GPUState<GPUBlendStateDesc>::Cache GPUState<GPUBlendStateDesc>::mCache{};
template<> GPUState<GPUDepthStencilStateDesc>::Cache GPUState<GPUDepthStencilStateDesc>::mCache{};
template<> GPUState<GPURasterizerStateDesc>::Cache GPUState<GPURasterizerStateDesc>::mCache{};
template<> GPUState<GPURenderTargetStateDesc>::Cache GPUState<GPURenderTargetStateDesc>::mCache{};
template<> GPUState<GPUVertexInputStateDesc>::Cache GPUState<GPUVertexInputStateDesc>::mCache{};

template class GPUState<GPUBlendStateDesc>;
template class GPUState<GPUDepthStencilStateDesc>;
template class GPUState<GPURasterizerStateDesc>;
template class GPUState<GPURenderTargetStateDesc>;
template class GPUState<GPUVertexInputStateDesc>;
