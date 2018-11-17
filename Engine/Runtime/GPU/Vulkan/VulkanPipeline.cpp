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

    // TODO.
}

static void ConvertInputAssemblyState(const GPUPipelineDesc&                  inDesc,
                                      VkPipelineInputAssemblyStateCreateInfo& outInputAssemblyInfo)
{
    outInputAssemblyInfo.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    outInputAssemblyInfo.topology = VulkanUtils::ConvertPrimitiveTopology(inDesc.topology);
}

static void ConvertViewportState(const GPUPipelineDesc&             inDesc,
                                 VkPipelineViewportStateCreateInfo& outViewportInfo)
{
    outViewportInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    outViewportInfo.viewportCount = 1;
    outViewportInfo.scissorCount  = 1;
}

static void ConvertRasterizerState(const GPUPipelineDesc&                  inDesc,
                                   VkPipelineRasterizationStateCreateInfo& outRasterizationInfo,
                                   VkPipelineMultisampleStateCreateInfo&   outMultisampleInfo)
{
    const auto& stateDesc = inDesc.rasterizerState->GetDesc();

    outRasterizationInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    outRasterizationInfo.depthClampEnable        = stateDesc.depthClampEnable;
    outRasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    outRasterizationInfo.polygonMode             = VulkanUtils::ConvertPolygonMode(stateDesc.polygonMode);
    outRasterizationInfo.cullMode                = VulkanUtils::ConvertCullMode(stateDesc.cullMode);
    outRasterizationInfo.frontFace               = VulkanUtils::ConvertFrontFace(stateDesc.frontFace);
    outRasterizationInfo.depthBiasEnable         = stateDesc.depthBiasEnable;

    outMultisampleInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    outMultisampleInfo.rasterizationSamples      = VK_SAMPLE_COUNT_1_BIT;
}

static void ConvertDepthStencilState(const GPUPipelineDesc&                 inDesc,
                                     VkPipelineDepthStencilStateCreateInfo& outDepthStencilInfo)
{
    const auto& stateDesc = inDesc.depthStencilState->GetDesc();

    outDepthStencilInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    outDepthStencilInfo.depthTestEnable       = stateDesc.depthTestEnable;
    outDepthStencilInfo.depthWriteEnable      = stateDesc.depthWriteEnable;
    outDepthStencilInfo.depthCompareOp        = VulkanUtils::ConvertCompareOp(stateDesc.depthCompareOp);
    outDepthStencilInfo.depthBoundsTestEnable = stateDesc.depthBoundsTestEnable;
    outDepthStencilInfo.stencilTestEnable     = stateDesc.stencilTestEnable;
    outDepthStencilInfo.front.failOp          = VulkanUtils::ConvertStencilOp(stateDesc.stencilFront.failOp);
    outDepthStencilInfo.front.passOp          = VulkanUtils::ConvertStencilOp(stateDesc.stencilFront.passOp);
    outDepthStencilInfo.front.depthFailOp     = VulkanUtils::ConvertStencilOp(stateDesc.stencilFront.depthFailOp);
    outDepthStencilInfo.front.compareOp       = VulkanUtils::ConvertCompareOp(stateDesc.stencilFront.compareOp);
    outDepthStencilInfo.front.compareMask     = stateDesc.stencilFront.compareMask;
    outDepthStencilInfo.front.writeMask       = stateDesc.stencilFront.writeMask;
    outDepthStencilInfo.front.reference       = stateDesc.stencilFront.reference;
    outDepthStencilInfo.back.failOp           = VulkanUtils::ConvertStencilOp(stateDesc.stencilBack.failOp);
    outDepthStencilInfo.back.passOp           = VulkanUtils::ConvertStencilOp(stateDesc.stencilBack.passOp);
    outDepthStencilInfo.back.depthFailOp      = VulkanUtils::ConvertStencilOp(stateDesc.stencilBack.depthFailOp);
    outDepthStencilInfo.back.compareOp        = VulkanUtils::ConvertCompareOp(stateDesc.stencilBack.compareOp);
    outDepthStencilInfo.back.compareMask      = stateDesc.stencilBack.compareMask;
    outDepthStencilInfo.back.writeMask        = stateDesc.stencilBack.writeMask;
    outDepthStencilInfo.back.reference        = stateDesc.stencilBack.reference;
}

static void ConvertBlendState(const GPUPipelineDesc&               inDesc,
                              VkPipelineColorBlendStateCreateInfo& outBlendInfo,
                              VkPipelineColorBlendAttachmentState* outBlendAttachments)
{
    const auto& stateDesc   = inDesc.blendState->GetDesc();
    const auto& rtStateDesc = inDesc.renderTargetState->GetDesc();

    outBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    for (size_t i = 0; i < kMaxRenderPassColourAttachments; i++)
    {
        if (rtStateDesc.colour[i] != PixelFormat::kUnknown)
        {
            outBlendInfo.attachmentCount = i + 1;
            outBlendInfo.pAttachments    = outBlendAttachments;

            outBlendAttachments[i].blendEnable         = stateDesc.attachments[i].enable;
            outBlendAttachments[i].srcColorBlendFactor = VulkanUtils::ConvertBlendFactor(stateDesc.attachments[i].srcColourFactor);
            outBlendAttachments[i].dstColorBlendFactor = VulkanUtils::ConvertBlendFactor(stateDesc.attachments[i].dstColourFactor);
            outBlendAttachments[i].colorBlendOp        = VulkanUtils::ConvertBlendOp(stateDesc.attachments[i].colourOp);
            outBlendAttachments[i].srcAlphaBlendFactor = VulkanUtils::ConvertBlendFactor(stateDesc.attachments[i].srcAlphaFactor);
            outBlendAttachments[i].dstAlphaBlendFactor = VulkanUtils::ConvertBlendFactor(stateDesc.attachments[i].dstAlphaFactor);
            outBlendAttachments[i].alphaBlendOp        = VulkanUtils::ConvertBlendOp(stateDesc.attachments[i].alphaOp);

            if (!stateDesc.attachments[i].maskR) outBlendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
            if (!stateDesc.attachments[i].maskG) outBlendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
            if (!stateDesc.attachments[i].maskB) outBlendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
            if (!stateDesc.attachments[i].maskA) outBlendAttachments[i].colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        }
        else
        {
            Assert(!stateDesc.attachments[i].enable);
        }
    }
}

VulkanPipeline::VulkanPipeline(VulkanDevice&          inDevice,
                               const GPUPipelineDesc& inDesc) :
    GPUPipeline (inDevice, inDesc),
    mHandle     (VK_NULL_HANDLE),
    mLayout     (VK_NULL_HANDLE)
{
    VulkanPipelineLayoutKey layoutKey;
    memcpy(layoutKey.argumentSetLayouts, inDesc.argumentSetLayouts, sizeof(layoutKey.argumentSetLayouts));
    mLayout = GetVulkanDevice().GetPipelineLayout(layoutKey);

    VkPipelineShaderStageCreateInfo        stageInfo[kGPUShaderStage_NumGraphics]            = {{}};
    VkPipelineVertexInputStateCreateInfo   vertexInputInfo                                   = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo                                 = {};
    VkPipelineViewportStateCreateInfo      viewportInfo                                      = {};
    VkPipelineRasterizationStateCreateInfo rasterizationInfo                                 = {};
    VkPipelineMultisampleStateCreateInfo   multisampleInfo                                   = {};
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo                                  = {};
    VkPipelineColorBlendAttachmentState    blendAttachments[kMaxRenderPassColourAttachments] = {{}};
    VkPipelineColorBlendStateCreateInfo    blendInfo                                         = {};
    VkGraphicsPipelineCreateInfo           createInfo                                        = {};

    ConvertShaderState(mDesc, stageInfo, createInfo.stageCount);
    ConvertVertexInputState(mDesc, vertexInputInfo);
    ConvertInputAssemblyState(mDesc, inputAssemblyInfo);
    ConvertViewportState(mDesc, viewportInfo);
    ConvertRasterizerState(mDesc, rasterizationInfo, multisampleInfo);
    ConvertDepthStencilState(mDesc, depthStencilInfo);
    ConvertBlendState(mDesc, blendInfo, blendAttachments);

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
    createInfo.layout              = mLayout;
    createInfo.renderPass          = GetVulkanDevice().GetRenderPass(mDesc.renderTargetState->GetDesc());
    createInfo.subpass             = 0;

    VulkanCheck(vkCreateGraphicsPipelines(GetVulkanDevice().GetHandle(),
                                          GetVulkanDevice().GetDriverPipelineCache(),
                                          1, &createInfo,
                                          nullptr,
                                          &mHandle));
}

VulkanPipeline::~VulkanPipeline()
{
    GetVulkanDevice().AddFrameCompleteCallback(
        [handle = mHandle, layout = mLayout] (VulkanDevice& inDevice)
        {
            vkDestroyPipelineLayout(inDevice.GetHandle(), layout, nullptr);
            vkDestroyPipeline(inDevice.GetHandle(), handle, nullptr);
        });
}
