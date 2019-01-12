/*
 * Copyright (C) 2018 Alex Smith
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

#include "GPU/GPUDeviceChild.h"

#include "Core/String.h"

/** A named child object of a GPUDevice. */
class GPUObject : public GPUDeviceChild
{
protected:
                            GPUObject(GPUDevice& inDevice);
    virtual                 ~GPUObject() {}

public:
    /**
     * Get/set a name for the object. This is for debugging purposes only. It
     * will be passed through to the underlying API and may be displayed in
     * tools (e.g. RenderDoc).
     */
    const std::string&      GetName() const { return mName; }
    void                    SetName(std::string inName);

protected:
    /** Callback when the name changes to pass this through to the API. */
    virtual void            UpdateName() {}

private:
    std::string             mName;

};

inline GPUObject::GPUObject(GPUDevice& inDevice) :
    GPUDeviceChild (inDevice)
{
}

inline void GPUObject::SetName(std::string inName)
{
    mName = std::move(inName);
    UpdateName();
}
