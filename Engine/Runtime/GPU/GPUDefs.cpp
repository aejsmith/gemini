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

#include "GPU/GPUContext.h"

#if GEMINI_GPU_MARKERS

GPUMarkerScope::GPUMarkerScope(GPUTransferContext& context,
                               const char* const   label) :
    mContext (context)
{
    mContext.BeginMarker(label);

    #if GEMINI_PROFILER
        MicroProfileGpuSetContext(&context);
        mToken = MicroProfileGetToken("GPU", label, 0xff0000, MicroProfileTokenTypeGpu);
        mTick  = MicroProfileEnter(mToken);
    #endif
}

GPUMarkerScope::GPUMarkerScope(GPUTransferContext& context,
                               const std::string&  label) :
    GPUMarkerScope (context, label.c_str())
{
}

GPUMarkerScope::~GPUMarkerScope()
{
    #if GEMINI_PROFILER
        MicroProfileGpuSetContext(&mContext);
        MicroProfileLeave(mToken, mTick);
    #endif

    mContext.EndMarker();
}

#endif
