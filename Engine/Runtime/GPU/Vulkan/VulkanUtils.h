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

#include "GPU/GPUDefs.h"

namespace VulkanUtils
{
    inline VkAttachmentLoadOp ConvertLoadOp(const GPULoadOp inOp)
    {
        switch (inOp)
        {
            case kGPULoadOp_Load:   return VK_ATTACHMENT_LOAD_OP_LOAD;
            case kGPULoadOp_Clear:  return VK_ATTACHMENT_LOAD_OP_CLEAR;

            default:
                UnreachableMsg("Unrecognised GPULoadOp");

        }
    }

    inline VkAttachmentStoreOp ConvertStoreOp(const GPUStoreOp inOp)
    {
        switch (inOp)
        {
            case kGPUStoreOp_Store:     return VK_ATTACHMENT_STORE_OP_STORE;
            case kGPUStoreOp_Discard:   return VK_ATTACHMENT_STORE_OP_DONT_CARE;

            default:
                UnreachableMsg("Unrecognised GPUStoreOp");

        }
    }

    inline VkShaderStageFlagBits ConvertShaderStage(const GPUShaderStage inStage)
    {
        switch (inStage)
        {
            case kGPUShaderStage_Vertex:    return VK_SHADER_STAGE_VERTEX_BIT;
            case kGPUShaderStage_Fragment:  return VK_SHADER_STAGE_FRAGMENT_BIT;
            case kGPUShaderStage_Compute:   return VK_SHADER_STAGE_COMPUTE_BIT;

            default:
                UnreachableMsg("Unrecognised GPUShaderStage");

        }
    }
}
