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

static void ConvertShaderState(const GPUPipelineDesc&           desc,
                               VkPipelineShaderStageCreateInfo* outStageInfos,
                               uint32_t&                        outStageCount)
{
    for (size_t stage = 0; stage < kGPUShaderStage_NumGraphics; stage++)
    {
        auto shader = static_cast<VulkanShader*>(desc.shaders[stage]);

        if (shader)
        {
            Assert(shader->GetStage() == stage);

            VkPipelineShaderStageCreateInfo& stageInfo = outStageInfos[outStageCount++];
            stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage  = VulkanUtils::ConvertShaderStage(shader->GetStage());
            stageInfo.module = shader->GetHandle();
            stageInfo.pName  = shader->GetFunction();
        }
    }
}

static void ConvertVertexInputState(const GPUPipelineDesc&                desc,
                                    VkPipelineVertexInputStateCreateInfo& outVertexInputInfo,
                                    VkVertexInputAttributeDescription*    outVertexAttributes,
                                    VkVertexInputBindingDescription*      outVertexBindings,
                                    uint32_t&                             outDummyBuffer)
{
    const auto& stateDesc = desc.vertexInputState->GetDesc();
    const auto shader     = static_cast<VulkanShader*>(desc.shaders[kGPUShaderStage_Vertex]);

    outVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    GPUVertexBufferBitset isReferenced;
    GPUVertexAttributeBitset needDummy;

    /* Iterate over the inputs the shader wants from SPIR-V reflection and
     * match to our attributes. */
    for (const auto& input : shader->GetVertexInputs())
    {
        Assert(input.semantic != kGPUAttributeSemantic_Unknown);

        bool matched = false;

        for (const auto& attribute : stateDesc.attributes)
        {
            matched = input.semantic == attribute.semantic && input.index == attribute.index;

            if (matched)
            {
                isReferenced.Set(attribute.buffer);

                outVertexInputInfo.pVertexAttributeDescriptions = outVertexAttributes;

                auto& outAttribute = outVertexAttributes[outVertexInputInfo.vertexAttributeDescriptionCount++];
                outAttribute.location = input.location;
                outAttribute.binding  = attribute.buffer;
                outAttribute.format   = VulkanUtils::ConvertAttributeFormat(attribute.format);
                outAttribute.offset   = attribute.offset;

                break;
            }
        }

        /* If we don't have a match, then we'll bind a dummy zero buffer to the
         * input. This was added as a quick and dirty solution to deal with
         * glTF meshes that do not provide UVs. The alternative is to generate
         * shader variants based on the meshes used with a material, but I'd
         * like to avoid this for now. */
        if (!matched)
        {
            LogWarning("Shader '%s' requires input %u[%u] which is not provided, will read as 0",
                       shader->GetName().c_str(),
                       input.semantic,
                       input.index);

            needDummy.Set(input.location);
        }
    }

    /* Add buffer definitions. */
    for (size_t bufferIndex = 0; bufferIndex < kMaxVertexAttributes; bufferIndex++)
    {
        if (isReferenced.Test(bufferIndex))
        {
            auto& buffer = stateDesc.buffers[bufferIndex];

            outVertexInputInfo.pVertexBindingDescriptions = outVertexBindings;

            auto& outBinding = outVertexBindings[outVertexInputInfo.vertexBindingDescriptionCount++];
            outBinding.binding   = bufferIndex;
            outBinding.stride    = buffer.stride;
            outBinding.inputRate = (buffer.perInstance)
                                       ? VK_VERTEX_INPUT_RATE_INSTANCE
                                       : VK_VERTEX_INPUT_RATE_VERTEX;
        }
        else if (needDummy.Any())
        {
            outDummyBuffer = bufferIndex;

            outVertexInputInfo.pVertexBindingDescriptions = outVertexBindings;

            /* Dummy buffer uses a stride of 0 so we only need to provide 1
             * value regardless of vertex count. */
            auto& outBinding = outVertexBindings[outVertexInputInfo.vertexBindingDescriptionCount++];
            outBinding.binding   = bufferIndex;
            outBinding.stride    = 0;
            outBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            for (size_t inputLocation = needDummy.FindFirst();
                 inputLocation < kMaxVertexAttributes;
                 inputLocation = needDummy.Clear(inputLocation).FindFirst())
            {
                outVertexInputInfo.pVertexAttributeDescriptions = outVertexAttributes;

                auto& outAttribute = outVertexAttributes[outVertexInputInfo.vertexAttributeDescriptionCount++];
                outAttribute.location = inputLocation;
                outAttribute.binding  = bufferIndex;
                outAttribute.format   = VK_FORMAT_R8G8B8A8_UNORM;
                outAttribute.offset   = 0;
            }
        }
    }

    AssertMsg(!needDummy.Any(), "No spare buffer slots for dummy buffer");
}

static void ConvertInputAssemblyState(const GPUPipelineDesc&                  desc,
                                      VkPipelineInputAssemblyStateCreateInfo& outInputAssemblyInfo)
{
    outInputAssemblyInfo.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    outInputAssemblyInfo.topology = VulkanUtils::ConvertPrimitiveTopology(desc.topology);
}

static void ConvertViewportState(const GPUPipelineDesc&             desc,
                                 VkPipelineViewportStateCreateInfo& outViewportInfo)
{
    outViewportInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    outViewportInfo.viewportCount = 1;
    outViewportInfo.scissorCount  = 1;
}

static void ConvertRasterizerState(const GPUPipelineDesc&                  desc,
                                   VkPipelineRasterizationStateCreateInfo& outRasterizationInfo,
                                   VkPipelineMultisampleStateCreateInfo&   outMultisampleInfo)
{
    const auto& stateDesc = desc.rasterizerState->GetDesc();

    outRasterizationInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    outRasterizationInfo.depthClampEnable        = stateDesc.depthClampEnable;
    outRasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    outRasterizationInfo.polygonMode             = VulkanUtils::ConvertPolygonMode(stateDesc.polygonMode);
    outRasterizationInfo.cullMode                = VulkanUtils::ConvertCullMode(stateDesc.cullMode);
    outRasterizationInfo.frontFace               = VulkanUtils::ConvertFrontFace(stateDesc.frontFace);
    outRasterizationInfo.depthBiasEnable         = stateDesc.depthBiasEnable;
    outRasterizationInfo.lineWidth               = 1.0f;

    outMultisampleInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    outMultisampleInfo.rasterizationSamples      = VK_SAMPLE_COUNT_1_BIT;
}

static void ConvertDepthStencilState(const GPUPipelineDesc&                 desc,
                                     VkPipelineDepthStencilStateCreateInfo& outDepthStencilInfo)
{
    const auto& stateDesc = desc.depthStencilState->GetDesc();

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

static void ConvertBlendState(const GPUPipelineDesc&               desc,
                              VkPipelineColorBlendStateCreateInfo& outBlendInfo,
                              VkPipelineColorBlendAttachmentState* outBlendAttachments)
{
    const auto& stateDesc   = desc.blendState->GetDesc();
    const auto& rtStateDesc = desc.renderTargetState->GetDesc();

    outBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    for (size_t i = 0; i < kMaxRenderPassColourAttachments; i++)
    {
        if (rtStateDesc.colour[i] != kPixelFormat_Unknown)
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

VulkanPipeline::VulkanPipeline(VulkanDevice&          device,
                               const GPUPipelineDesc& desc) :
    GPUPipeline        (device, desc),
    mHandle            (VK_NULL_HANDLE),
    mLayout            (VK_NULL_HANDLE),
    mDummyVertexBuffer (kMaxVertexAttributes)
{
    VulkanPipelineLayoutKey layoutKey;
    memcpy(layoutKey.argumentSetLayouts, desc.argumentSetLayouts, sizeof(layoutKey.argumentSetLayouts));
    mLayout = GetVulkanDevice().GetPipelineLayout(layoutKey);

    VkPipelineShaderStageCreateInfo        stageInfo[kGPUShaderStage_NumGraphics]            = {{}};
    VkVertexInputAttributeDescription      vertexAttributes[kMaxVertexAttributes]            = {{}};
    VkVertexInputBindingDescription        vertexBindings[kMaxVertexAttributes]              = {{}};
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
    ConvertVertexInputState(mDesc, vertexInputInfo, vertexAttributes, vertexBindings, mDummyVertexBuffer);
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
        [handle = mHandle] (VulkanDevice& device)
        {
            vkDestroyPipeline(device.GetHandle(), handle, nullptr);
        });
}

VulkanComputePipeline::VulkanComputePipeline(VulkanDevice&                 device,
                                             const GPUComputePipelineDesc& desc) :
    GPUComputePipeline  (device, desc),
    mHandle             (VK_NULL_HANDLE),
    mLayout             (VK_NULL_HANDLE)
{
    VulkanPipelineLayoutKey layoutKey;
    memcpy(layoutKey.argumentSetLayouts, desc.argumentSetLayouts, sizeof(layoutKey.argumentSetLayouts));
    mLayout = GetVulkanDevice().GetPipelineLayout(layoutKey);

    const auto shader = static_cast<VulkanShader*>(desc.shader);

    VkComputePipelineCreateInfo createInfo = {};
    createInfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    createInfo.stage.module = shader->GetHandle();
    createInfo.stage.pName  = shader->GetFunction();
    createInfo.layout       = mLayout;

    VulkanCheck(vkCreateComputePipelines(GetVulkanDevice().GetHandle(),
                                         GetVulkanDevice().GetDriverPipelineCache(),
                                         1, &createInfo,
                                         nullptr,
                                         &mHandle));
}

VulkanComputePipeline::~VulkanComputePipeline()
{
    GetVulkanDevice().AddFrameCompleteCallback(
        [handle = mHandle] (VulkanDevice& device)
        {
            vkDestroyPipeline(device.GetHandle(), handle, nullptr);
        });
}
