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

#if !GEMINI_BUILD_RELEASE

#define MICROPROFILE_IMPL 1

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wshadow"
#endif

#endif

#include "Engine/Profiler.h"

#include "Core/Thread.h"

#include "Engine/Engine.h"
#include "Engine/EngineSettings.h"

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUQueryPool.h"

#if GEMINI_PROFILER

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

SINGLETON_IMPL(Profiler);

static constexpr uint32_t kGPUFrameQueries = MICROPROFILE_GPU_MAX_QUERIES / MICROPROFILE_GPU_FRAMES;

Profiler::Profiler() :
    mGPUQueryPool (nullptr),
    mGPUFrame     (0),
    mGPUFramePut  (0),
    mGPUSubmitted {},
    mGPUResults   {}
{
    MicroProfileInit();
    MicroProfileSetEnableAllGroups(true);
    MicroProfileOnThreadCreate("Main");

    if (Engine::Get().GetSettings().profilerWebServer)
    {
        MicroProfileWebServerStart();
    }
}

Profiler::~Profiler()
{
    delete mGPUQueryPool;
}

void Profiler::GPUInit(OnlyCalledBy<Engine>)
{
    g_MicroProfile.GPU.Shutdown          = GPUShutdown;
    g_MicroProfile.GPU.Flip              = GPUFlip;
    g_MicroProfile.GPU.InsertTimer       = GPUInsertTimer;
    g_MicroProfile.GPU.GetTimeStamp      = GPUGetTimestamp;
    g_MicroProfile.GPU.GetTicksPerSecond = GPUTicksPerSecond;
    g_MicroProfile.GPU.GetTickReference  = GPUTickReference;

    GPUQueryPoolDesc desc;
    desc.type  = kGPUQueryType_Timestamp;
    desc.count = MICROPROFILE_GPU_MAX_QUERIES;

    mGPUQueryPool = GPUDevice::Get().CreateQueryPool(desc);
}

void Profiler::EndFrame(OnlyCalledBy<Engine>)
{
    MicroProfileFlip();
}

void Profiler::GPUShutdown()
{
}

uint32_t Profiler::GPUFlip()
{
    Profiler& self = Get();

    const uint32_t frameIndex     = self.mGPUFrame % MICROPROFILE_GPU_FRAMES;
    const uint32_t frameTimestamp = GPUInsertTimer(&GPUGraphicsContext::Get());
    const uint32_t framePut       = std::min(self.mGPUFramePut.load(), kGPUFrameQueries);

    self.mGPUSubmitted[frameIndex] = framePut;
    self.mGPUFramePut.store(0);
    self.mGPUFrame++;

    if (self.mGPUFrame >= MICROPROFILE_GPU_FRAMES)
    {
        const uint64_t pendingFrame      = self.mGPUFrame - MICROPROFILE_GPU_FRAMES;
        const uint32_t pendingFrameIndex = pendingFrame % MICROPROFILE_GPU_FRAMES;
        const uint32_t pendingFrameStart = pendingFrameIndex * kGPUFrameQueries;
        const uint32_t pendingFrameCount = self.mGPUSubmitted[pendingFrameIndex];

        if (pendingFrameCount)
        {
            self.mGPUQueryPool->GetResults(pendingFrameStart,
                                           pendingFrameCount,
                                           GPUQueryPool::kGetResults_Wait | GPUQueryPool::kGetResults_Reset,
                                           &self.mGPUResults[pendingFrameStart]);
        }
    }

    return frameTimestamp;
}

uint32_t Profiler::GPUInsertTimer(void* const context)
{
    Profiler& self = Get();

    // TODO: Multithreading support, support for command lists.
    Assert(Thread::IsMain());
    Assert(context);

    const uint32_t index = self.mGPUFramePut.fetch_add(1);

    if (index >= kGPUFrameQueries)
    {
        return static_cast<uint32_t>(-1);
    }

    const uint32_t queryIndex = ((self.mGPUFrame % MICROPROFILE_GPU_FRAMES) * kGPUFrameQueries) + index;

    auto gpuContext = static_cast<GPUTransferContext*>(context);
    gpuContext->Query(self.mGPUQueryPool, queryIndex);

    return queryIndex;
}

uint64_t Profiler::GPUGetTimestamp(const uint32_t index)
{
    Profiler& self = Get();
    return self.mGPUResults[index];
}

uint64_t Profiler::GPUTicksPerSecond()
{
    /* GPU backend always uses nanoseconds. */
    return 1000000000ull;
}

bool Profiler::GPUTickReference(int64_t* outCPU, int64_t* outGPU)
{
    /* MicroProfile doesn't call this at the moment. */
    Assert(false);
    return true;
}

#endif
