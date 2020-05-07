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

#pragma once

#include "GPU/GPUQueryPool.h"

#include "VulkanDeviceChild.h"

class VulkanQueryPool final : public GPUQueryPool,
                              public VulkanDeviceChild<VulkanQueryPool>
{
public:
                            VulkanQueryPool(VulkanDevice&           device,
                                            const GPUQueryPoolDesc& desc);

                            ~VulkanQueryPool();

    void                    Reset(const uint16_t start,
                                  const uint16_t count) override;

    bool                    GetResults(const uint16_t start,
                                       const uint16_t count,
                                       const uint32_t flags,
                                       uint64_t*      outData) override;

    VkQueryPool             GetHandle() const { return mHandle; }

private:
    VkQueryPool             mHandle;

};
