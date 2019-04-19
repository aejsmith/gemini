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

#include "Core/Singleton.h"

#include "GPU/GPUBuffer.h"
#include "GPU/GPUTexture.h"

#include "Render/RenderDefs.h"

#include <list>

class Engine;
class RenderGraph;
class RenderOutput;

class RenderManager : public Singleton<RenderManager>
{
public:
                                RenderManager();
                                ~RenderManager();

public:
    /** Render all outputs for the frame. */
    void                        Render(OnlyCalledBy<Engine>);

    /**
     * Interface with RenderOutput.
     */

    // TODO: Want an order for outputs (e.g. render to texture included in
    // main scene would need to be rendered first), a way to disable outputs
    // (don't want to render to texture when it's not going to be needed in
    // main scene).
    void                        RegisterOutput(RenderOutput* const inOutput,
                                               OnlyCalledBy<RenderOutput>);
    void                        UnregisterOutput(RenderOutput* const inOutput,
                                                 OnlyCalledBy<RenderOutput>);

    /**
     * Interface with RenderGraph.
     */

    /**
     * Allocate transient resources. Returns a resource matching the specified
     * descriptor. Will reuse resources from previous frames if available.
     * Resources that go unused for a certain period will be freed.
     */
    GPUResource*                GetTransientBuffer(const GPUBufferDesc& inDesc,
                                                   OnlyCalledBy<RenderGraph>);
    GPUResource*                GetTransientTexture(const GPUTextureDesc& inDesc,
                                                    OnlyCalledBy<RenderGraph>);

private:
    struct TransientResource
    {
        GPUResource*            resource;

        /**
         * Start time of the frame (Engine::GetFrameStartTime()) in which the
         * resource was last used. Indicates when we should free the resource,
         * and also whether the resource is available for reuse in the current
         * frame.
         */
        uint32_t                lastUsedFrameStartTime;
    };

    struct TransientBuffer : TransientResource
    {
        GPUBufferDesc           desc;
    };

    struct TransientTexture : TransientResource
    {
        GPUTextureDesc          desc;
    };

private:
    std::list<RenderOutput*>    mOutputs;

    std::list<TransientBuffer>  mTransientBuffers;
    std::list<TransientTexture> mTransientTextures;

};
