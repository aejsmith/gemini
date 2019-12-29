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

#include "Render/DeferredRenderPipeline.h"

#include "Engine/DebugManager.h"
#include "Engine/DebugWindow.h"

#include "Render/EntityDrawList.h"
#include "Render/RenderContext.h"
#include "Render/RenderWorld.h"
#include "Render/ShaderManager.h"
#include "Render/TonemapPass.h"

/** Debug overlay window with debug visualisation etc. options. */
class DeferredRenderPipelineWindow : public DebugWindow
{
public:
                                DeferredRenderPipelineWindow(DeferredRenderPipeline* const inPipeline);

protected:
    void                        Render() override;

private:
    DeferredRenderPipeline*     mPipeline;

};

struct DeferredRenderContext : public RenderContext
{
    using RenderContext::RenderContext;

    CullResults                 cullResults;
    EntityDrawList              opaqueDrawList;

    /**
     * Render graph resource handles (always refer to latest version unless
     * otherwise stated).
     */

    /** Main colour output target. */
    RenderResourceHandle        colourTexture;

    /** Main depth buffer target. */
    RenderResourceHandle        depthTexture;

    /** G-Buffer targets. */
    RenderResourceHandle        GBuffer0Texture;
    RenderResourceHandle        GBuffer1Texture;
    RenderResourceHandle        GBuffer2Texture;
};

DeferredRenderPipelineWindow::DeferredRenderPipelineWindow(DeferredRenderPipeline* const inPipeline) :
    DebugWindow ("Render", "Render Pipeline"),
    mPipeline   (inPipeline)
{
}

void DeferredRenderPipelineWindow::Render()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300 - 10, 30), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once);

    if (!Begin())
    {
        return;
    }

    if (ImGui::CollapsingHeader("Debug visualisation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Draw entity bounding boxes", &mPipeline->mDrawEntityBoundingBoxes);
        ImGui::Checkbox("Draw light volumes", &mPipeline->mDrawLightVolumes);
    }

    ImGui::End();
}

DeferredRenderPipeline::DeferredRenderPipeline() :
    mTonemapPass                (new TonemapPass),

    mDrawEntityBoundingBoxes    (false),
    mDrawLightVolumes           (false),
    mDebugWindow                (new DeferredRenderPipelineWindow(this))
{
}

DeferredRenderPipeline::~DeferredRenderPipeline()
{
}

void DeferredRenderPipeline::SetName(std::string inName)
{
    RenderPipeline::SetName(std::move(inName));

    mDebugWindow->SetTitle(StringUtils::Format("Render Pipeline '%s'", GetName().c_str()));
}

void DeferredRenderPipeline::Render(const RenderWorld&         inWorld,
                                    const RenderView&          inView,
                                    RenderGraph&               inGraph,
                                    const RenderResourceHandle inTexture,
                                    RenderResourceHandle&      outNewTexture)
{
    DeferredRenderContext* const context = inGraph.NewTransient<DeferredRenderContext>(inWorld, inView);

    CreateResources(context, inGraph, inTexture);
    BuildDrawLists(context);
    RenderGBuffer(context, inGraph);

    // TODO: Lighting

    /* Tonemap and gamma correct onto the output texture. */
    mTonemapPass->AddPass(inGraph,
                          context->colourTexture,
                          inTexture,
                          outNewTexture);

    /* Render debug primitives for the view. */
    DebugManager::Get().RenderPrimitives(inView,
                                         inGraph,
                                         outNewTexture,
                                         outNewTexture);
}

void DeferredRenderPipeline::CreateResources(DeferredRenderContext* const inContext,
                                             RenderGraph&                 inGraph,
                                             const RenderResourceHandle   inOutputTexture) const
{
    const RenderTextureDesc& outputDesc = inGraph.GetTextureDesc(inOutputTexture);

    RenderTextureDesc textureDesc;
    textureDesc.width          = outputDesc.width;
    textureDesc.height         = outputDesc.height;
    textureDesc.depth          = outputDesc.depth;

    textureDesc.name           = "DeferredColour";
    textureDesc.format         = kColourFormat;
    inContext->colourTexture   = inGraph.CreateTexture(textureDesc);

    textureDesc.name           = "DeferredDepth";
    textureDesc.format         = kDepthFormat;
    inContext->depthTexture    = inGraph.CreateTexture(textureDesc);

    textureDesc.name           = "DeferredGBuffer0";
    textureDesc.format         = kGBuffer0Format;
    inContext->GBuffer0Texture = inGraph.CreateTexture(textureDesc);

    textureDesc.name           = "DeferredGBuffer1";
    textureDesc.format         = kGBuffer1Format;
    inContext->GBuffer1Texture = inGraph.CreateTexture(textureDesc);

    textureDesc.name           = "DeferredGBuffer2";
    textureDesc.format         = kGBuffer2Format;
    inContext->GBuffer2Texture = inGraph.CreateTexture(textureDesc);
}

void DeferredRenderPipeline::BuildDrawLists(DeferredRenderContext* const inContext) const
{
    /* Get the visible entities and lights. */
    inContext->GetWorld().Cull(inContext->GetView(), inContext->cullResults);

    /* Build a draw list for the opaque G-Buffer pass. */
    inContext->opaqueDrawList.Reserve(inContext->cullResults.entities.size());
    for (const RenderEntity* entity : inContext->cullResults.entities)
    {
        if (entity->SupportsPassType(kShaderPassType_DeferredOpaque))
        {
            const GPUPipeline* const pipeline = entity->GetPipeline(kShaderPassType_DeferredOpaque);

            const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
            EntityDrawCall& drawCall = inContext->opaqueDrawList.Add(sortKey);

            entity->GetDrawCall(kShaderPassType_DeferredOpaque, *inContext, drawCall);

            if (mDrawEntityBoundingBoxes)
            {
                DebugManager::Get().DrawPrimitive(entity->GetWorldBoundingBox(), glm::vec3(0.0f, 0.0f, 1.0f));
            }
        }
    }

    /* Sort them. */
    inContext->opaqueDrawList.Sort();

    if (mDrawLightVolumes)
    {
        for (const RenderLight* light : inContext->cullResults.lights)
        {
            light->DrawDebugPrimitive();
        }
    }
}

void DeferredRenderPipeline::RenderGBuffer(DeferredRenderContext* const inContext,
                                           RenderGraph&                 inGraph) const
{
    RenderGraphPass& opaquePass = inGraph.AddPass("DeferredOpaque", kRenderGraphPassType_Render);

    /* Colour output is bound as target 3 for emissive materials to output
     * directly to.
     *
     * TODO: We should mask output 3 in the pipeline state for non-emissive
     * materials. */
    opaquePass.SetColour(0, inContext->GBuffer0Texture, &inContext->GBuffer0Texture);
    opaquePass.SetColour(1, inContext->GBuffer1Texture, &inContext->GBuffer1Texture);
    opaquePass.SetColour(2, inContext->GBuffer2Texture, &inContext->GBuffer2Texture);
    opaquePass.SetColour(3, inContext->colourTexture,   &inContext->colourTexture);

    opaquePass.ClearColour(0, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearColour(1, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearColour(2, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearColour(3, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    opaquePass.SetDepthStencil(inContext->depthTexture, kGPUResourceState_DepthStencilWrite, &inContext->depthTexture);

    opaquePass.ClearDepth(1.0f);

    inContext->opaqueDrawList.Draw(opaquePass);
}
