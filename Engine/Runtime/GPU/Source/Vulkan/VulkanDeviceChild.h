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

#include "VulkanDefs.h"

class VulkanDevice;
class VulkanInstance;

/**
 * Mixin class for any class with a GetDevice() method returning a GPUDevice,
 * to cast it to a VulkanDevice.
 */
template <typename T>
class VulkanDeviceChild
{
public:
    VulkanDevice&               GetVulkanDevice() const;
    VulkanInstance&             GetVulkanInstance() const;

};

template <typename T>
inline VulkanDevice& VulkanDeviceChild<T>::GetVulkanDevice() const
{
    return static_cast<VulkanDevice&>(static_cast<const T*>(this)->GetDevice());
}

template <typename T>
inline VulkanInstance& VulkanDeviceChild<T>::GetVulkanInstance() const
{
    return GetVulkanDevice().GetInstance();
}
