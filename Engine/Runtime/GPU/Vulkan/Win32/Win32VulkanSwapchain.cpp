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

#define VK_USE_PLATFORM_WIN32_KHR 1

#include <SDL.h>
#include <SDL_syswm.h>

#include "../VulkanSwapchain.h"

#include "../VulkanDevice.h"
#include "../VulkanInstance.h"

#include "Engine/Window.h"

static void GetWMInfo(const Window&  window,
                      SDL_SysWMinfo& outWMInfo)
{
    SDL_VERSION(&outWMInfo.version);
    if (!SDL_GetWindowWMInfo(window.GetSDLWindow(), &outWMInfo))
    {
        Fatal("Failed to get SDL WM info: %s", SDL_GetError());
    }
}

const char* VulkanSwapchain::GetSurfaceExtensionName()
{
    return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
}

bool VulkanSwapchain::CheckPresentationSupport(const VkPhysicalDevice device,
                                               const uint32_t         queueFamily)
{
    auto vkGetPhysicalDeviceWin32PresentationSupportKHR =
        VulkanInstance::Get().Load<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>("vkGetPhysicalDeviceWin32PresentationSupportKHR", true);

    return vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueFamily);
}

void VulkanSwapchain::CreateSurface()
{
    SDL_SysWMinfo wmInfo;
    GetWMInfo(GetWindow(), wmInfo);

    Assert(wmInfo.subsystem == SDL_SYSWM_WINDOWS);

    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd      = wmInfo.info.win.window;

    auto vkCreateWin32SurfaceKHR =
        VulkanInstance::Get().Load<PFN_vkCreateWin32SurfaceKHR>("vkCreateWin32SurfaceKHR", true);

    VulkanCheck(vkCreateWin32SurfaceKHR(GetVulkanInstance().GetHandle(),
                                        &createInfo,
                                        nullptr,
                                        &mSurfaceHandle));
}
