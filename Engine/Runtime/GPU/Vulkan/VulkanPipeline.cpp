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

#include "VulkanPipeline.h"

#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanUtils.h"

static VkDynamicState kDynamicStates[] =
{
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_DEPTH_BIAS,
    VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    VK_DYNAMIC_STATE_DEPTH_BOUNDS,
};

static VkPipelineDynamicStateCreateInfo kDynamicStateInfo =
{
    VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    nullptr,
    0,
    ArraySize(kDynamicStates),
    kDynamicStates
};

static constexpr char kShaderEntryPointName[] = "main";

static void ConvertShaderState(const GPUPipelineDesc&           inDesc,
                               VkPipelineShaderStageCreateInfo* outStageInfos,
                               uint32_t&                        outStageCount)
{
    for (size_t stage = 0; stage < kGPUShaderStage_NumGraphics; stage++)
    {
        auto shader = static_cast<VulkanShader*>(inDesc.shaders[stage]);

        if (shader)
        {
            Assert(shader->GetStage() == stage);

            VkPipelineShaderStageCreateInfo& stageInfo = outStageInfos[outStageCount++];
            stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage  = VulkanUtils::ConvertShaderStage(shader->GetStage());
            stageInfo.module = shader->GetHandle();
            stageInfo.pName  = kShaderEntryPointName;
        }
    }
}

static void ConvertVertexInputState(const GPUPipelineDesc&                inDesc,
                                    VkPipelineVertexInputStateCreateInfo& outVertexInputInfo)
{
    outVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
}

static void ConvertInputAssemblyState(const GPUPipelineDesc&                  inDesc,
                                      VkPipelineInputAssemblyStateCreateInfo& outInputAssemblyInfo)
{
    outInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}

static void ConvertViewportState(const GPUPipelineDesc&             inDesc,
                                 VkPipelineViewportStateCreateInfo& outViewportInfo)
{
    outViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
}

static void ConvertRasterizerState(const GPUPipelineDesc&                  inDesc,
                                   VkPipelineRasterizationStateCreateInfo& outRasterizationInfo,
                                   VkPipelineMultisampleStateCreateInfo&   outMultisampleInfo)
{
    outRasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    outMultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
}

static void ConvertDepthStencilState(const GPUPipelineDesc&                 inDesc,
                                     VkPipelineDepthStencilStateCreateInfo& outDepthStencilInfo)
{
    outDepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

static void ConvertBlendState(const GPUPipelineDesc&               inDesc,
                              VkPipelineColorBlendStateCreateInfo& outBlendInfo)
{
    outBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
}


VulkanPipeline::VulkanPipeline(VulkanDevice&          inDevice,
                               const GPUPipelineDesc& inDesc) :
    GPUPipeline (inDevice, inDesc),
    mHandle     (VK_NULL_HANDLE)
{
    VkPipelineShaderStageCreateInfo        stageInfo[kGPUShaderStage_NumGraphics] = {{}};
    VkPipelineVertexInputStateCreateInfo   vertexInputInfo                        = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo                      = {};
    VkPipelineViewportStateCreateInfo      viewportInfo                           = {};
    VkPipelineRasterizationStateCreateInfo rasterizationInfo                      = {};
    VkPipelineMultisampleStateCreateInfo   multisampleInfo                        = {};
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo                       = {};
    VkPipelineColorBlendStateCreateInfo    blendInfo                              = {};
    VkGraphicsPipelineCreateInfo           createInfo                             = {};

    ConvertShaderState(mDesc, stageInfo, createInfo.stageCount);
    ConvertVertexInputState(mDesc, vertexInputInfo);
    ConvertInputAssemblyState(mDesc, inputAssemblyInfo);
    ConvertViewportState(mDesc, viewportInfo);
    ConvertRasterizerState(mDesc, rasterizationInfo, multisampleInfo);
    ConvertDepthStencilState(mDesc, depthStencilInfo);
    ConvertBlendState(mDesc, blendInfo);

    createInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pStages             = stageInfo;
    createInfo.pVertexInputState   = &vertexInputInfo;
    createInfo.pInputAssemblyState = &inputAssemblyInfo;
    createInfo.pViewportState      = &viewportInfo;
    createInfo.pRasterizationState = &rasterizationInfo;
    createInfo.pMultisampleState   = &multisampleInfo;
    createInfo.pDepthStencilState  = &depthStencilInfo;
    createInfo.pColorBlendState    = &blendInfo;
    createInfo.pDynamicState       = &kDynamicStateInfo;

    //createInfo.layout
    //createInfo.renderPass (RenderTargetState)
    //createInfo.subpass = 0;

    VulkanCheck(vkCreateGraphicsPipelines(GetVulkanDevice().GetHandle(),
                                          GetVulkanDevice().GetDriverPipelineCache(),
                                          1, &createInfo,
                                          nullptr,
                                          &mHandle));
}

VulkanPipeline::~VulkanPipeline()
{
    vkDestroyPipeline(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}
