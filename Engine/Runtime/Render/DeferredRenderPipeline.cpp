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

class DeferredRenderPipelineWindow : public DebugWindow
{
public:
                                DeferredRenderPipelineWindow(DeferredRenderPipeline* const inPipeline);

protected:
    void                        Render() override;

private:
    DeferredRenderPipeline*     mPipeline;

};

class DeferredRenderContext : public RenderContext
{
public:
    using RenderContext::RenderContext;

    CullResults                 cullResults;
    EntityDrawList              opaqueDrawList;
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

    if (ImGui::CollapsingHeader("Debug Visualisation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Draw entity bounding boxes", &mPipeline->mDrawEntityBoundingBoxes);
        ImGui::Checkbox("Draw light volumes", &mPipeline->mDrawLightVolumes);
    }

    ImGui::End();
}

DeferredRenderPipeline::DeferredRenderPipeline() :
    clearColour                 (0.0f, 0.0f, 0.0f, 1.0f),

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

    /* Get the visible entities. */
    inWorld.Cull(inView, context->cullResults);

    /* Build a draw list for the opaque G-Buffer pass. */
    context->opaqueDrawList.Reserve(context->cullResults.entities.size());
    for (const RenderEntity* entity : context->cullResults.entities)
    {
        if (entity->SupportsPassType(kShaderPassType_DeferredOpaque))
        {
            const GPUPipeline* const pipeline = entity->GetPipeline(kShaderPassType_DeferredOpaque);

            const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
            EntityDrawCall& drawCall = context->opaqueDrawList.Add(sortKey);

            entity->GetDrawCall(kShaderPassType_DeferredOpaque, *context, drawCall);

            if (mDrawEntityBoundingBoxes)
            {
                DebugManager::Get().DrawPrimitive(entity->GetWorldBoundingBox(), glm::vec3(0.0f, 0.0f, 1.0f));
            }
        }
    }

    /* Sort them. */
    context->opaqueDrawList.Sort();

    if (mDrawLightVolumes)
    {
        for (const RenderLight* light : context->cullResults.lights)
        {
            light->DrawDebugPrimitive();
        }
    }

    // TODO: Temporary, output G-Buffer pass to the main colour texture.

    RenderGraphPass& opaquePass = inGraph.AddPass("DeferredOpaque", kRenderGraphPassType_Render);

    RenderTextureDesc colourTextureDesc(inGraph.GetTextureDesc(inTexture));
    colourTextureDesc.format = kColourFormat;

    RenderTextureDesc depthTextureDesc(inGraph.GetTextureDesc(inTexture));
    depthTextureDesc.format = kDepthFormat;

    RenderResourceHandle colourTexture = inGraph.CreateTexture(colourTextureDesc);
    RenderResourceHandle depthTexture  = inGraph.CreateTexture(depthTextureDesc);

    opaquePass.SetColour(0, colourTexture, &colourTexture);
    opaquePass.SetDepthStencil(depthTexture, kGPUResourceState_DepthStencilWrite);

    opaquePass.ClearColour(0, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearDepth(1.0f);

    context->opaqueDrawList.Draw(opaquePass);

    // TODO: Apply clear colour to main colour target.

    /* Tonemap and gamma correct onto the output texture. */
    mTonemapPass->AddPass(inGraph,
                          colourTexture,
                          inTexture,
                          outNewTexture);

    /* Render debug primitives for the view. */
    DebugManager::Get().RenderPrimitives(inView,
                                         inGraph,
                                         outNewTexture,
                                         outNewTexture);
}
