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

#include "VulkanDefs.h"

#include <string>

namespace VulkanFuncs
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

    #define DEFINE_VULKAN_FUNC(name) \
        PFN_##name name;

    ENUMERATE_VULKAN_NO_INSTANCE_FUNCS(DEFINE_VULKAN_FUNC);
    ENUMERATE_VULKAN_INSTANCE_FUNCS(DEFINE_VULKAN_FUNC);
    ENUMERATE_VULKAN_INSTANCE_EXTENSION_FUNCS(DEFINE_VULKAN_FUNC);
    ENUMERATE_VULKAN_DEVICE_FUNCS(DEFINE_VULKAN_FUNC);
    ENUMERATE_VULKAN_DEVICE_EXTENSION_FUNCS(DEFINE_VULKAN_FUNC);
}

void VulkanCheckFailed(const char*    call,
                       const VkResult result)
{
    std::string string(call);
    string.erase(string.find('('));
    Fatal("%s failed: %d", string.c_str(), result);
}
