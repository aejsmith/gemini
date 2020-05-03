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

#include "VulkanDefs.h"

#include "Core/Singleton.h"

#include <vector>

class VulkanInstance : public Singleton<VulkanInstance>
{
public:
    /** Instance capability flags. */
    enum Caps : uint32_t
    {
        kCap_Validation  = (1 << 0),
        kCap_DebugReport = (1 << 1),
    };

    using LayerList           = std::vector<const char*>;

public:
                                VulkanInstance();
                                ~VulkanInstance();

public:
    VkInstance                  GetHandle() const           { return mHandle; }
    const LayerList&            GetEnabledLayers() const    { return mEnabledLayers; }

    bool                        HasCap(const Caps cap) const
                                    { return (mCaps & cap) == cap; }

    template <typename Function>
    Function                    Load(const char* name,
                                     const bool  required);

private:
    /* Implemented by platform-specific code. */
    void                        OpenLoader();
    void                        CloseLoader();

    void                        CreateInstance();

private:
    void*                       mLoaderHandle;
    VkInstance                  mHandle;
    uint32_t                    mCaps;

    std::vector<const char*>    mEnabledLayers;

    #if GEMINI_VULKAN_VALIDATION
    VkDebugReportCallbackEXT    mDebugReportCallback;
    #endif

};

template <typename Function>
inline Function VulkanInstance::Load(const char* name,
                                     const bool  required)
{
    auto func = reinterpret_cast<Function>(vkGetInstanceProcAddr(mHandle, name));

    if (!func && required)
    {
        Fatal("Failed to load Vulkan function '%s'", name);
    }

    return func;
}
