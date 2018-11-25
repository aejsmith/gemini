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

#include "Core/RefCounted.h"
#include "Core/String.h"

/**
 * A reference counted child object of a GPUDevice.
 *
 * Most functions in the GPU layer take raw pointers/references to GPUObject-
 * derived classes. This is to avoid adding/releasing references around every
 * call. It is expected that if a object is passed to a function, then the
 * caller guarantees that a reference is held somewhere else for the duration
 * of the call.
 */
class GPUObject : public GPUDeviceChild,
                  public RefCounted
{
protected:
                            GPUObject(GPUDevice& inDevice);
                            ~GPUObject() {}

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
