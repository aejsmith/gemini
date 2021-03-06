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

#include "Render/RenderManager.h"

#include "Core/Time.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"

#include "GPU/GPUDevice.h"

#include "Render/RenderGraph.h"
#include "Render/RenderLayer.h"
#include "Render/RenderOutput.h"

/** Time that a transient resource will go unused for before we free it. */
static constexpr uint64_t kTransientResourceFreePeriod = 2 * kNanosecondsPerSecond;

SINGLETON_IMPL(RenderManager);

RenderManager::RenderManager()
{
    {
        GPUArgumentSetLayoutDesc argumentLayoutDesc(kViewEntityArgumentCount);
        argumentLayoutDesc.arguments[kViewEntityArguments_ViewConstants]   = kGPUArgumentType_Constants;
        argumentLayoutDesc.arguments[kViewEntityArguments_EntityConstants] = kGPUArgumentType_Constants;

        mViewEntityArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));
        mViewEntityArgumentSet       = GPUDevice::Get().CreateArgumentSet(mViewEntityArgumentSetLayout, nullptr);
    }

    {
        GPUArgumentSetLayoutDesc argumentLayoutDesc(1);
        argumentLayoutDesc.arguments[kViewEntityArguments_ViewConstants] = kGPUArgumentType_Constants;

        mViewArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));
    }
}

RenderManager::~RenderManager()
{
    auto FreeTransientResources = [&] (auto& transientResources)
    {
        while (!transientResources.empty())
        {
            auto& resource = transientResources.front();
            transientResources.pop_front();

            delete resource.resource;
        }
    };

    FreeTransientResources(mTransientBuffers);
    FreeTransientResources(mTransientTextures);

    delete mViewEntityArgumentSet;
}

void RenderManager::InitAssets(OnlyCalledBy<Engine>)
{
    mDummyBlackTexture2D = AssetManager::Get().Load<Texture2D>("Engine/Textures/DummyBlack2D");
    Assert(mDummyBlackTexture2D);

    mDummyWhiteTexture2D = AssetManager::Get().Load<Texture2D>("Engine/Textures/DummyWhite2D");
    Assert(mDummyWhiteTexture2D);

    mDummyBlackTexture2DView = mDummyBlackTexture2D->GetResourceView();
    mDummyWhiteTexture2DView = mDummyWhiteTexture2D->GetResourceView();

    GPUResourceViewDesc arrayDesc;
    arrayDesc.type   = kGPUResourceViewType_Texture2DArray;
    arrayDesc.usage  = kGPUResourceUsage_ShaderRead;
    arrayDesc.format = mDummyBlackTexture2D->GetFormat();

    mDummyBlackTexture2DArrayView = GPUDevice::Get().CreateResourceView(mDummyBlackTexture2D->GetTexture(), arrayDesc);
    mDummyWhiteTexture2DArrayView = GPUDevice::Get().CreateResourceView(mDummyWhiteTexture2D->GetTexture(), arrayDesc);
}

void RenderManager::Render(OnlyCalledBy<Engine>)
{
    RENDER_PROFILER_FUNC_SCOPE();

    const uint64_t frameStartTime = Engine::Get().GetFrameStartTime();

    /* Free transient resources that have gone unused long enough. */
    auto FreeUnusedTransientResources = [&] (auto& transientResources)
    {
        for (auto it = transientResources.begin(); it != transientResources.end(); )
        {
            if (frameStartTime - it->lastUsedFrameStartTime >= kTransientResourceFreePeriod)
            {
                delete it->resource;
                it = transientResources.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    FreeUnusedTransientResources(mTransientBuffers);
    FreeUnusedTransientResources(mTransientTextures);

    /* Build a render graph for all our outputs and execute it. */
    RenderGraph graph;

    {
        RENDER_PROFILER_SCOPE("AddPasses");

        for (RenderOutput* output : mOutputs)
        {
            output->AddPasses(graph, {});
        }
    }

    graph.Execute();
}

void RenderManager::RegisterOutput(RenderOutput* const output,
                                   OnlyCalledBy<RenderOutput>)
{
    mOutputs.emplace_back(output);
}

void RenderManager::UnregisterOutput(RenderOutput* const output,
                                     OnlyCalledBy<RenderOutput>)
{
    mOutputs.remove(output);
}

GPUResource* RenderManager::GetTransientBuffer(const GPUBufferDesc& desc,
                                               OnlyCalledBy<RenderGraph>)
{
    const uint64_t frameStartTime = Engine::Get().GetFrameStartTime();

    /* Look for an existing resource to use. */
    for (TransientBuffer& buffer : mTransientBuffers)
    {
        if (buffer.desc == desc &&
            buffer.lastUsedFrameStartTime != frameStartTime)
        {
            buffer.lastUsedFrameStartTime = frameStartTime;
            return buffer.resource;
        }
    }

    /* Create a new one. */
    mTransientBuffers.emplace_back();
    TransientBuffer& buffer = mTransientBuffers.back();
    buffer.desc                   = desc;
    buffer.lastUsedFrameStartTime = frameStartTime;
    buffer.resource               = GPUDevice::Get().CreateBuffer(desc);

    return buffer.resource;
}

GPUResource* RenderManager::GetTransientTexture(const GPUTextureDesc& desc,
                                                OnlyCalledBy<RenderGraph>)
{
    const uint64_t frameStartTime = Engine::Get().GetFrameStartTime();

    /* Look for an existing resource to use. */
    for (TransientTexture& texture : mTransientTextures)
    {
        if (texture.desc == desc &&
            texture.lastUsedFrameStartTime != frameStartTime)
        {
            texture.lastUsedFrameStartTime = frameStartTime;
            return texture.resource;
        }
    }

    /* Create a new one. */
    mTransientTextures.emplace_back();
    TransientTexture& texture = mTransientTextures.back();
    texture.desc                   = desc;
    texture.lastUsedFrameStartTime = frameStartTime;
    texture.resource               = GPUDevice::Get().CreateTexture(desc);

    return texture.resource;
}
