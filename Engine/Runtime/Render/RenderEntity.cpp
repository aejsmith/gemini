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

#include "Render/RenderEntity.h"

#include "GPU/GPUConstantPool.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUPipeline.h"

#include "Render/EntityDrawList.h"
#include "Render/Material.h"
#include "Render/RenderContext.h"
#include "Render/RenderManager.h"

RenderEntity::RenderEntity(const Renderer& inRenderer,
                           Material&       inMaterial) :
    mRenderer   (inRenderer),
    mMaterial   (inMaterial),
    mPipelines  {}
{
}

RenderEntity::~RenderEntity()
{
}

void RenderEntity::CreatePipelines()
{
    const ShaderTechnique* const technique = mMaterial.GetShaderTechnique();

    for (uint32_t passType = 0; passType < kShaderPassTypeCount; passType++)
    {
        const ShaderPass* const pass = technique->GetPass(static_cast<ShaderPassType>(passType));

        if (pass)
        {
            GPUPipelineDesc pipelineDesc;

            for (uint32_t stage = 0; stage < kGPUShaderStage_NumGraphics; stage++)
            {
                pipelineDesc.shaders[stage] = pass->GetShader(static_cast<GPUShaderStage>(stage));
            }

            pipelineDesc.argumentSetLayouts[kArgumentSet_ViewEntity] = RenderManager::Get().GetViewEntityArgumentSetLayout();
            // TODO: Material.

            pipelineDesc.blendState        = pass->GetBlendState();
            pipelineDesc.depthStencilState = pass->GetDepthStencilState();
            pipelineDesc.rasterizerState   = pass->GetRasterizerState();
            pipelineDesc.renderTargetState = pass->GetRenderTargetState();
            pipelineDesc.vertexInputState  = GetVertexInputState();
            pipelineDesc.topology          = GetPrimitiveTopology();

            mPipelines[passType] = GPUDevice::Get().GetPipeline(pipelineDesc);
        }
    }
}

void RenderEntity::SetTransform(const Transform& inTransform)
{
    mTransform        = inTransform;
    mWorldBoundingBox = GetLocalBoundingBox().Transform(inTransform);
}

void RenderEntity::GetDrawCall(const ShaderPassType inPassType,
                               const RenderContext& inContext,
                               EntityDrawCall&      outDrawCall) const
{
    Assert(SupportsPassType(inPassType));

    outDrawCall.pipeline = mPipelines[inPassType];

    /* Set view/entity arguments. */
    {
        EntityConstants entityConstants;
        entityConstants.transformMatrix = GetTransform().GetMatrix();
        entityConstants.position        = GetTransform().GetPosition();

        auto& arguments = outDrawCall.arguments[kArgumentSet_ViewEntity];
        arguments.argumentSet                = RenderManager::Get().GetViewEntityArgumentSet();
        arguments.constants[0].argumentIndex = kViewEntityArguments_ViewConstants;
        arguments.constants[0].constants     = inContext.GetView().GetConstants();
        arguments.constants[1].argumentIndex = kViewEntityArguments_EntityConstants;
        arguments.constants[1].constants     = GPUDevice::Get().GetConstantPool().Write(&entityConstants, sizeof(entityConstants));
    }

    GetGeometry(outDrawCall);
}
