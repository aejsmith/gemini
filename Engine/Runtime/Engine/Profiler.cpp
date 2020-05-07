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

#include "Engine/Engine.h"
#include "Engine/EngineSettings.h"

#if GEMINI_PROFILER_ENABLED

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

SINGLETON_IMPL(Profiler);

Profiler::Profiler()
{
    MicroProfileInit();
    MicroProfileSetEnableAllGroups(true);
    MicroProfileOnThreadCreate("Main");

    if (Engine::Get().GetSettings().profilerWebServer)
    {
        MicroProfileWebServerStart();
    }
}

void Profiler::EndFrame()
{
    MicroProfileFlip();
}

#endif
