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

#include <SDL.h>

#ifdef SDL_VIDEO_DRIVER_X11
    #define VK_USE_PLATFORM_XCB_KHR 1

    /* Avoid conflicts with our own Window type. */
    #define Window X11Window

    #include <X11/Xlib-xcb.h>
#endif

#include <SDL_syswm.h>

#ifdef SDL_VIDEO_DRIVER_X11
    #undef Window
#endif

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
    SDL_SysWMinfo wmInfo;
    GetWMInfo(MainWindow::Get(), wmInfo);

    const char* extension = nullptr;

    switch (wmInfo.subsystem)
    {
        #ifdef SDL_VIDEO_DRIVER_X11
            case SDL_SYSWM_X11:
                extension = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
                break;
        #endif

        default:
            Fatal("SDL video subsystem %d is not supported", wmInfo.subsystem);

    }

    return extension;
}

bool VulkanSwapchain::CheckPresentationSupport(const VkPhysicalDevice device,
                                               const uint32_t         queueFamily)
{
    SDL_SysWMinfo wmInfo;
    GetWMInfo(MainWindow::Get(), wmInfo);

    VkBool32 result = VK_FALSE;

    switch (wmInfo.subsystem)
    {
        #ifdef SDL_VIDEO_DRIVER_X11
            case SDL_SYSWM_X11:
            {
                xcb_connection_t* connection = XGetXCBConnection(wmInfo.info.x11.display);

                xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(connection,
                                                                                      wmInfo.info.x11.window);

                xcb_generic_error_t* error;
                xcb_get_window_attributes_reply_t* reply = xcb_get_window_attributes_reply(connection,
                                                                                           cookie,
                                                                                           &error);

                if (error)
                {
                    Fatal("Failed to obtain XCB window attributes");
                }

                auto vkGetPhysicalDeviceXcbPresentationSupportKHR =
                    VulkanInstance::Get().Load<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>(
                        "vkGetPhysicalDeviceXcbPresentationSupportKHR",
                        true);

                result = vkGetPhysicalDeviceXcbPresentationSupportKHR(device,
                                                                      queueFamily,
                                                                      connection,
                                                                      reply->visual);

                free(reply);

                break;
            }
        #endif

        default:
            Fatal("SDL video subsystem %d is not supported", wmInfo.subsystem);

    }

    return result;
}

void VulkanSwapchain::CreateSurface()
{
    SDL_SysWMinfo wmInfo;
    GetWMInfo(GetWindow(), wmInfo);

    switch (wmInfo.subsystem)
    {
        #ifdef SDL_VIDEO_DRIVER_X11
            case SDL_SYSWM_X11:
            {
                VkXcbSurfaceCreateInfoKHR createInfo = {};
                createInfo.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
                createInfo.connection = XGetXCBConnection(wmInfo.info.x11.display);
                createInfo.window     = wmInfo.info.x11.window;

                auto vkCreateXcbSurfaceKHR =
                    VulkanInstance::Get().Load<PFN_vkCreateXcbSurfaceKHR>(
                        "vkCreateXcbSurfaceKHR",
                        true);

                VulkanCheck(vkCreateXcbSurfaceKHR(GetVulkanInstance().GetHandle(),
                                                  &createInfo,
                                                  nullptr,
                                                  &mSurfaceHandle));

                break;
            }
        #endif

        default:
            Fatal("SDL video subsystem %d is not supported", wmInfo.subsystem);

    }
}
