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

#include "VulkanShader.h"

#include "VulkanDevice.h"

#include "Core/Utility.h"

#include <spirv_reflect.h>

static const char* const kSemanticNames[] =
{
    /* kGPUAttributeSemantic_Unknown      */ "UNKNOWN",
    /* kGPUAttributeSemantic_Binormal     */ "BINORMAL",
    /* kGPUAttributeSemantic_BlendIndices */ "BLENDINDICES",
    /* kGPUAttributeSemantic_BlendWeight  */ "BLENDWEIGHTS",
    /* kGPUAttributeSemantic_Colour       */ "COLOR",
    /* kGPUAttributeSemantic_Normal       */ "NORMAL",
    /* kGPUAttributeSemantic_Position     */ "POSITION",
    /* kGPUAttributeSemantic_Tangent      */ "TANGENT",
    /* kGPUAttributeSemantic_TexCoord     */ "TEXCOORD",
};

VulkanShader::VulkanShader(VulkanDevice&        inDevice,
                           const GPUShaderStage inStage,
                           GPUShaderCode        inCode,
                           const std::string&   inFunction) :
    GPUShader   (inDevice,
                 inStage,
                 std::move(inCode)),
    mHandle     (VK_NULL_HANDLE),
    mFunction   (inFunction)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = GetCode().size() * sizeof(uint32_t);
    createInfo.pCode    = GetCode().data();

    VulkanCheck(vkCreateShaderModule(GetVulkanDevice().GetHandle(),
                                     &createInfo,
                                     nullptr,
                                     &mHandle));

    Reflect();
}

VulkanShader::~VulkanShader()
{
    vkDestroyShaderModule(GetVulkanDevice().GetHandle(), mHandle, nullptr);
}

void VulkanShader::Reflect()
{
    if (GetStage() != kGPUShaderStage_Vertex)
    {
        /* Only reflection we need right now is vertex inputs. */
        return;
    }

    SpvReflectResult result;

    SpvReflectShaderModule module = {};
    result = spvReflectCreateShaderModule(GetCode().size() * sizeof(uint32_t),
                                          GetCode().data(),
                                          &module);
    Assert(result == SPV_REFLECT_RESULT_SUCCESS);

    auto guard = MakeScopeGuard([&] { spvReflectDestroyShaderModule(&module); });

    uint32_t count = 0;
    result = spvReflectEnumerateInputVariables(&module, &count, nullptr);
    Assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectInterfaceVariable*> inputs(count);
    result = spvReflectEnumerateInputVariables(&module, &count, inputs.data());
    Assert(result == SPV_REFLECT_RESULT_SUCCESS);

    mVertexInputs.reserve(count);
    for (const auto& input : inputs)
    {
        if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
        {
            continue;
        }

        mVertexInputs.emplace_back();
        auto& vertexInput = mVertexInputs.back();

        vertexInput.location = input->location;
        vertexInput.semantic = kGPUAttributeSemantic_Unknown;

        const size_t inputSemanticLen = strlen(input->semantic);

        for (size_t i = 0; i < ArraySize(kSemanticNames); i++)
        {
            const char* const semanticName = kSemanticNames[i];
            const size_t semanticLen       = strlen(semanticName);

            /* The index is part of the string, but for index 0 it is valid to
             * omit the index (e.g. "POSITION" and "POSITION0" mean the same
             * thing). */
            if (inputSemanticLen >= semanticLen &&
                memcmp(input->semantic, semanticName, semanticLen) == 0 &&
                (inputSemanticLen == semanticLen || isdigit(input->semantic[semanticLen])))
            {
                if (inputSemanticLen > semanticLen)
                {
                    char* end;
                    vertexInput.index = strtoul(input->semantic + semanticLen, &end, 10);

                    if (end != input->semantic + inputSemanticLen)
                    {
                        break;
                    }
                }

                vertexInput.semantic = static_cast<GPUAttributeSemantic>(i);
                break;
            }
        }

        if (vertexInput.semantic == kGPUAttributeSemantic_Unknown)
        {
            Fatal("Shader has unknown vertex input semantic '%s'", input->semantic);
        }
    }
}

void VulkanShader::UpdateName()
{
    GetVulkanDevice().UpdateName(mHandle,
                                 VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT,
                                 GetName());
}
