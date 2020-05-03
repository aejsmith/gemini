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

#include "GPU/GPUResource.h"

struct GPUResourceViewDesc
{
    GPUResourceViewType         type;

    /**
     * Usage flag indicating what this view will be used for. Only one flag can
     * be set, and the resource the view refers to must allow this usage.
     */
    GPUResourceUsage            usage;

    /**
     * View format. Must be kPixelFormat_Unknown for raw buffer views,
     * otherwise must be compatible with the underlying resource format.
     */
    PixelFormat                 format          = kPixelFormat_Unknown;

    /**
     * Base mip level and mip count. mipCount must be 1 for anything other
     * than kGPUResourceUsage_ShaderRead views.
     */
    uint32_t                    mipOffset       = 0;
    uint32_t                    mipCount        = 1;

    /**
     * Element offset and element count. For a buffer view, this specifies the
     * byte offset into the buffer and byte size to view. For texture views, it
     * specifies the array offset and layer count. For cube views, these must
     * be a multiple of 6.
     */
    uint32_t                    elementOffset   = 0;
    uint32_t                    elementCount    = 1;
};

/**
 * A view into a part of a resource, used for binding resources to shaders and
 * for use as a render target. A view's resource must be kept alive as long as
 * the view.
 */
class GPUResourceView : public GPUObject
{
protected:
                                GPUResourceView(GPUResource&               resource,
                                                const GPUResourceViewDesc& desc);

public:
                                ~GPUResourceView();

    GPUResource&                GetResource() const         { return *mResource; }

    GPUResourceViewType         GetType() const             { return mDesc.type; }
    GPUResourceUsage            GetUsage() const            { return mDesc.usage; }
    PixelFormat                 GetFormat() const           { return mDesc.format; }
    uint32_t                    GetMipOffset() const        { return mDesc.mipOffset; }
    uint32_t                    GetMipCount() const         { return mDesc.mipCount; }
    uint32_t                    GetElementOffset() const    { return mDesc.elementOffset; }
    uint32_t                    GetElementCount() const     { return mDesc.elementCount; }

    /**
     * Get a GPUSubresourceRange structure corresponding to this view, suitable
     * for use e.g. in a GPUResourceBarrier. If this view refers to a buffer,
     * will return 0 for offsets and 1 for count.
     */
    GPUSubresourceRange         GetSubresourceRange() const;

protected:
    GPUResource*                mResource;
    const GPUResourceViewDesc   mDesc;

};

inline GPUSubresourceRange GPUResourceView::GetSubresourceRange() const
{
    if (mResource->IsBuffer())
    {
        return { 0, 1, 0, 1 };
    }
    else
    {
        return { mDesc.mipOffset, mDesc.mipCount, mDesc.elementOffset, mDesc.elementCount };
    }
}
