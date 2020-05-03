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

#include "GPU/GPUArgumentSet.h"

#include "GPU/GPUResourceView.h"

GPUArgumentSetLayout::GPUArgumentSetLayout(GPUDevice&                 device,
                                           GPUArgumentSetLayoutDesc&& desc) :
    GPUDeviceChild  (device),
    mDesc           (std::move(desc))
{
    Assert(GetArgumentCount() <= kMaxArgumentsPerSet);

    mIsConstantOnly = true;

    for (const auto argument : GetArguments())
    {
        if (argument != kGPUArgumentType_Constants)
        {
            mIsConstantOnly = false;
        }
    }
}

GPUArgumentSetLayout::~GPUArgumentSetLayout()
{
}

GPUArgumentSet::GPUArgumentSet(GPUDevice&                    device,
                               const GPUArgumentSetLayoutRef layout,
                               const GPUArgument* const      arguments) :
    GPUObject   (device),
    mLayout     (layout)
{
    ValidateArguments(layout, arguments);
}

GPUArgumentSet::~GPUArgumentSet()
{
}

#if GEMINI_BUILD_DEBUG

void GPUArgumentSet::ValidateArguments(const GPUArgumentSetLayoutRef layout,
                                       const GPUArgument* const      arguments)
{
    Assert(layout);

    const auto& argumentTypes = layout->GetArguments();

    for (size_t i = 0; i < layout->GetArgumentCount(); i++)
    {
        if (argumentTypes[i] == kGPUArgumentType_Constants)
        {
            Assert(!arguments || (!arguments[i].view && !arguments[i].sampler));
        }
        else
        {
            Assert(arguments);

            const auto view    = arguments[i].view;
            const auto sampler = arguments[i].sampler;

            /* Must have only 1 of a view or sampler. */
            Assert(!view != !sampler);

            switch (argumentTypes[i])
            {
                case kGPUArgumentType_Buffer:
                    Assert(view);
                    Assert(view->GetType() == kGPUResourceViewType_Buffer);
                    Assert(view->GetResource().GetUsage() & kGPUResourceUsage_ShaderRead);
                    break;

                case kGPUArgumentType_RWBuffer:
                    Assert(view);
                    Assert(view->GetType() == kGPUResourceViewType_Buffer);
                    Assert(view->GetResource().GetUsage() & kGPUResourceUsage_ShaderWrite);
                    break;

                case kGPUArgumentType_Texture:
                    Assert(view);
                    Assert(view->GetResource().IsTexture());
                    Assert(view->GetResource().GetUsage() & kGPUResourceUsage_ShaderRead);
                    break;

                case kGPUArgumentType_RWTexture:
                    Assert(view);
                    Assert(view->GetResource().IsTexture());
                    Assert(view->GetResource().GetUsage() & kGPUResourceUsage_ShaderWrite);
                    break;

                case kGPUArgumentType_TextureBuffer:
                    Assert(view);
                    Assert(view->GetType() == kGPUResourceViewType_TextureBuffer);
                    Assert(view->GetResource().GetUsage() & kGPUResourceUsage_ShaderRead);
                    break;

                case kGPUArgumentType_RWTextureBuffer:
                    Assert(view);
                    Assert(view->GetType() == kGPUResourceViewType_TextureBuffer);
                    Assert(view->GetResource().GetUsage() & kGPUResourceUsage_ShaderWrite);
                    break;

                case kGPUArgumentType_Sampler:
                    Assert(sampler);
                    break;

                default:
                    UnreachableMsg("Unknown GPUArgumentType");
                    break;

            }
        }
    }
}

#endif
