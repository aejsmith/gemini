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

#include "Core/Singleton.h"

#define GEMINI_PROFILER (!GEMINI_BUILD_RELEASE)

#if GEMINI_PROFILER

#define MICROPROFILE_PER_THREAD_BUFFER_SIZE     (2 * 1024 * 1024)
#define MICROPROFILE_PER_THREAD_GPU_BUFFER_SIZE (1 * 1024 * 1024)
#define MICROPROFILE_MAX_FRAME_HISTORY          22
#define MICROPROFILE_LABEL_BUFFER_SIZE          (1 * 1024 * 1024)
#define MICROPROFILE_GPU_MAX_QUERIES            8192

/* TODO: Support queries within command lists. For now they're only supported
 * on the contexts which can only be used from the main thread. */
//#define MICROPROFILE_GPU_TIMERS_MULTITHREADED   1

#include "microprofile.h"

#include <atomic>

class Engine;
class GPUQueryPool;
class ProfilerWindow;

class Profiler : public Singleton<Profiler>
{
public:
                            Profiler();
                            ~Profiler();

    void                    GPUInit(OnlyCalledBy<Engine>);
    void                    WindowInit(OnlyCalledBy<Engine>);

    void                    EndFrame(OnlyCalledBy<Engine>);

private:
    static void             GPUShutdown();
    static uint32_t         GPUFlip();
    static uint32_t         GPUInsertTimer(void* const context);
    static uint64_t         GPUGetTimestamp(const uint32_t index);
    static uint64_t         GPUTicksPerSecond();
    static bool             GPUTickReference(int64_t* outCPU, int64_t* outGPU);

private:
    UPtr<GPUQueryPool>      mGPUQueryPool;
    uint64_t                mGPUFrame;
    std::atomic<uint32_t>   mGPUFramePut;
    uint32_t                mGPUSubmitted[MICROPROFILE_GPU_FRAMES];
    uint64_t                mGPUResults[MICROPROFILE_GPU_MAX_QUERIES];

    UPtr<ProfilerWindow>    mWindow;

};

#define PROFILER_SCOPE(group, timer, colour)    MICROPROFILE_SCOPEI(group, timer, colour)
#define PROFILER_FUNC_SCOPE(group, colour)      MICROPROFILE_SCOPEI(group, __func__, colour)

#else // GEMINI_PROFILER

#define PROFILER_SCOPE(group, timer, colour)
#define PROFILER_FUNC_SCOPE(group, colour)

#endif
