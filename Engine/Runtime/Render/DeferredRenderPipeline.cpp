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

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUCommandList.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUPipeline.h"
#include "GPU/GPUStagingResource.h"

#include "Render/EntityDrawList.h"
#include "Render/RenderContext.h"
#include "Render/RenderManager.h"
#include "Render/RenderWorld.h"
#include "Render/ShaderManager.h"
#include "Render/TonemapPass.h"

#include "../../Shaders/DeferredDefs.h"

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

    uint32_t                    tilesWidth;
    uint32_t                    tilesHeight;
    uint32_t                    tilesCount;

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

    /** Light buffers. */
    RenderResourceHandle        lightParamsBuffer;
    RenderResourceHandle        visibleLightCountBuffer;
    RenderResourceHandle        visibleLightsBuffer;
};

DeferredRenderPipeline::DeferredRenderPipeline() :
    mTonemapPass                (new TonemapPass),

    mDrawEntityBoundingBoxes    (false),
    mDrawLightVolumes           (false),
    mDebugWindow                (new DeferredRenderPipelineWindow(this))
{
    CreateShaders();
}

DeferredRenderPipeline::~DeferredRenderPipeline()
{
}

void DeferredRenderPipeline::CreateShaders()
{
    /* Light culling compute shader. */
    {
        mCullingShader = ShaderManager::Get().GetShader("Engine/DeferredCulling.hlsl", "CSMain", kGPUShaderStage_Compute);

        GPUArgumentSetLayoutDesc argumentLayoutDesc(kDeferredCullingArgumentsCount);
        argumentLayoutDesc.arguments[kDeferredCullingArguments_DepthTexture]      = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredCullingArguments_LightParams]       = kGPUArgumentType_Buffer;
        argumentLayoutDesc.arguments[kDeferredCullingArguments_VisibleLightCount] = kGPUArgumentType_RWBuffer;
        argumentLayoutDesc.arguments[kDeferredCullingArguments_VisibleLights]     = kGPUArgumentType_RWBuffer;
        argumentLayoutDesc.arguments[kDeferredCullingArguments_Constants]         = kGPUArgumentType_Constants;

        const GPUArgumentSetLayoutRef argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

        GPUComputePipelineDesc pipelineDesc;
        pipelineDesc.argumentSetLayouts[kArgumentSet_ViewEntity]      = RenderManager::Get().GetViewArgumentSetLayout();
        pipelineDesc.argumentSetLayouts[kArgumentSet_DeferredCulling] = argumentLayout;
        pipelineDesc.shader                                           = mCullingShader;

        mCullingPipeline.reset(GPUDevice::Get().CreateComputePipeline(pipelineDesc));
    }

    /* Lighting compute shader. */
    {
        mLightingShader = ShaderManager::Get().GetShader("Engine/DeferredLighting.hlsl", "CSMain", kGPUShaderStage_Compute);

        GPUArgumentSetLayoutDesc argumentLayoutDesc(kDeferredLightingArgumentsCount);
        argumentLayoutDesc.arguments[kDeferredLightingArguments_GBuffer0Texture]   = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_GBuffer1Texture]   = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_GBuffer2Texture]   = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_DepthTexture]      = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_LightParams]       = kGPUArgumentType_Buffer;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_VisibleLightCount] = kGPUArgumentType_Buffer;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_VisibleLights]     = kGPUArgumentType_Buffer;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_ColourTexture]     = kGPUArgumentType_RWTexture;
        argumentLayoutDesc.arguments[kDeferredLightingArguments_Constants]         = kGPUArgumentType_Constants;

        const GPUArgumentSetLayoutRef argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

        GPUComputePipelineDesc pipelineDesc;
        pipelineDesc.argumentSetLayouts[kArgumentSet_ViewEntity]       = RenderManager::Get().GetViewArgumentSetLayout();
        pipelineDesc.argumentSetLayouts[kArgumentSet_DeferredLighting] = argumentLayout;
        pipelineDesc.shader                                            = mLightingShader;

        mLightingPipeline.reset(GPUDevice::Get().CreateComputePipeline(pipelineDesc));
    }
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
    DeferredRenderContext* const context = inGraph.NewTransient<DeferredRenderContext>(inGraph, inWorld, inView);

    /* Get the visible entities and lights. */
    context->GetWorld().Cull(context->GetView(), context->cullResults);

    CreateResources(context, inTexture);
    BuildDrawLists(context);
    PrepareLights(context);
    AddGBufferPasses(context);
    AddCullingPass(context);
    AddLightingPass(context);

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
                                             const RenderResourceHandle   inOutputTexture) const
{
    RenderGraph& graph = inContext->GetGraph();

    const RenderTextureDesc& outputDesc = graph.GetTextureDesc(inOutputTexture);

    /* Calculate output dimensions in number of tiles. */
    inContext->tilesWidth  = RoundUp(outputDesc.width,  static_cast<uint32_t>(kDeferredTileSize)) / kDeferredTileSize;
    inContext->tilesHeight = RoundUp(outputDesc.height, static_cast<uint32_t>(kDeferredTileSize)) / kDeferredTileSize;
    inContext->tilesCount  = inContext->tilesWidth * inContext->tilesHeight;

    RenderTextureDesc textureDesc;
    textureDesc.width                  = outputDesc.width;
    textureDesc.height                 = outputDesc.height;
    textureDesc.depth                  = outputDesc.depth;

    textureDesc.name                   = "DeferredColour";
    textureDesc.format                 = kColourFormat;
    inContext->colourTexture           = graph.CreateTexture(textureDesc);

    textureDesc.name                   = "DeferredDepth";
    textureDesc.format                 = kDepthFormat;
    inContext->depthTexture            = graph.CreateTexture(textureDesc);

    textureDesc.name                   = "DeferredGBuffer0";
    textureDesc.format                 = kGBuffer0Format;
    inContext->GBuffer0Texture         = graph.CreateTexture(textureDesc);

    textureDesc.name                   = "DeferredGBuffer1";
    textureDesc.format                 = kGBuffer1Format;
    inContext->GBuffer1Texture         = graph.CreateTexture(textureDesc);

    textureDesc.name                   = "DeferredGBuffer2";
    textureDesc.format                 = kGBuffer2Format;
    inContext->GBuffer2Texture         = graph.CreateTexture(textureDesc);

    RenderBufferDesc bufferDesc;

    bufferDesc.name                    = "DeferredLightParams";
    bufferDesc.size                    = sizeof(LightParams) * kDeferredMaxLightCount;
    inContext->lightParamsBuffer       = graph.CreateBuffer(bufferDesc);

    bufferDesc.name                    = "DeferredVisibleLightCount";
    bufferDesc.size                    = sizeof(uint32_t) * inContext->tilesCount;
    inContext->visibleLightCountBuffer = graph.CreateBuffer(bufferDesc);

    bufferDesc.name                    = "DeferredVisibleLights";
    bufferDesc.size                    = sizeof(uint32_t) * kDeferredVisibleLightsTileEntryCount * inContext->tilesCount;
    inContext->visibleLightsBuffer     = graph.CreateBuffer(bufferDesc);
}

void DeferredRenderPipeline::BuildDrawLists(DeferredRenderContext* const inContext) const
{
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

    inContext->opaqueDrawList.Sort();
}

void DeferredRenderPipeline::PrepareLights(DeferredRenderContext* const inContext) const
{
    auto& lightList = inContext->cullResults.lights;

    /* We have fixed size resources and light indices that can only cope with a
     * certain number of lights, ignore any lights that exceed this. */
    if (lightList.size() > kDeferredMaxLightCount)
    {
        LogWarning("Visible light count %zu exceeds limit %zu, truncating list",
                   lightList.size(),
                   kDeferredMaxLightCount);

        lightList.resize(kDeferredMaxLightCount);
    }

    /* Fill a buffer with parameters for the visible lights. TODO: Investigate
     * performance of uploading this buffer each frame, may be a problem for
     * larger light counts. */
    GPUStagingBuffer stagingBuffer(kGPUStagingAccess_Write, sizeof(LightParams) * lightList.size());
    auto lightParams = stagingBuffer.MapWrite<LightParams>();

    for (size_t i = 0; i < lightList.size(); i++)
    {
        const RenderLight* const light = lightList[i];

        light->GetLightParams(lightParams[i]);

        if (mDrawLightVolumes)
        {
            light->DrawDebugPrimitive();
        }
    }

    stagingBuffer.Finalise();

    inContext->GetGraph().AddUploadPass("DeferredLightParamsUpload",
                                        inContext->lightParamsBuffer,
                                        0,
                                        std::move(stagingBuffer),
                                        &inContext->lightParamsBuffer);
}

void DeferredRenderPipeline::AddGBufferPasses(DeferredRenderContext* const inContext) const
{
    RenderGraphPass& opaquePass = inContext->GetGraph().AddPass("DeferredOpaque", kRenderGraphPassType_Render);

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

void DeferredRenderPipeline::AddCullingPass(DeferredRenderContext* const inContext) const
{
    RenderGraph& graph = inContext->GetGraph();

    RenderGraphPass& pass = graph.AddPass("DeferredCulling", kRenderGraphPassType_Compute);

    RenderViewDesc viewDesc;

    viewDesc.type                             = kGPUResourceViewType_Texture2D;
    viewDesc.state                            = kGPUResourceState_ComputeShaderRead;
    const RenderViewHandle depthHandle        = pass.CreateView(inContext->depthTexture, viewDesc);

    viewDesc.type                             = kGPUResourceViewType_Buffer;
    viewDesc.elementCount                     = graph.GetBufferDesc(inContext->lightParamsBuffer).size;
    const RenderViewHandle paramsHandle       = pass.CreateView(inContext->lightParamsBuffer, viewDesc);

    viewDesc.state                            = kGPUResourceState_ComputeShaderWrite;
    viewDesc.elementCount                     = graph.GetBufferDesc(inContext->visibleLightCountBuffer).size;
    const RenderViewHandle visibleCountHandle = pass.CreateView(inContext->visibleLightCountBuffer, viewDesc, &inContext->visibleLightCountBuffer);

    viewDesc.elementCount                     = graph.GetBufferDesc(inContext->visibleLightsBuffer).size;
    const RenderViewHandle visibleHandle      = pass.CreateView(inContext->visibleLightsBuffer, viewDesc, &inContext->visibleLightsBuffer);

    pass.SetFunction([=] (const RenderGraph&     inGraph,
                          const RenderGraphPass& inPass,
                          GPUComputeCommandList& inCmdList)
    {
        inCmdList.SetPipeline(mCullingPipeline.get());

        GPUArgument arguments[kDeferredCullingArgumentsCount];
        arguments[kDeferredCullingArguments_DepthTexture].view      = inPass.GetView(depthHandle);
        arguments[kDeferredCullingArguments_LightParams].view       = inPass.GetView(paramsHandle);
        arguments[kDeferredCullingArguments_VisibleLightCount].view = inPass.GetView(visibleCountHandle);
        arguments[kDeferredCullingArguments_VisibleLights].view     = inPass.GetView(visibleHandle);

        inCmdList.SetArguments(kArgumentSet_DeferredCulling, arguments);

        DeferredCullingConstants constants;
        constants.tileDimensions = glm::uvec2(inContext->tilesWidth, inContext->tilesHeight);
        constants.lightCount     = inContext->cullResults.lights.size();

        inCmdList.WriteConstants(kArgumentSet_DeferredCulling,
                                 kDeferredCullingArguments_Constants,
                                 &constants,
                                 sizeof(constants));

        inCmdList.SetConstants(kArgumentSet_ViewEntity,
                               kViewEntityArguments_ViewConstants,
                               inContext->GetView().GetConstants());

        inCmdList.Dispatch(inContext->tilesWidth, inContext->tilesHeight, 1);
    });
}

void DeferredRenderPipeline::AddLightingPass(DeferredRenderContext* const inContext) const
{
    /* TODO: Investigate performance of compute vs pixel shader. Pixel may be
     * beneficial due to colour compression - AMD pre-Navi can't do compressed
     * UAV writes, same for at least NVIDIA Maxwell (unsure about anything
     * newer). However a pixel shader would probably need some tricks to
     * scalarize access to the tile/light data, as we have no guarantees on
     * whether or not pixel shader wavefronts can cross tile boundaries, and
     * the compiler would not be able to scalarize by itself. */

    RenderGraph& graph = inContext->GetGraph();

    RenderGraphPass& pass = graph.AddPass("DeferredLighting", kRenderGraphPassType_Compute);

    RenderViewDesc viewDesc;

    viewDesc.type                             = kGPUResourceViewType_Texture2D;
    viewDesc.state                            = kGPUResourceState_ComputeShaderRead;
    const RenderViewHandle GBuffer0Handle     = pass.CreateView(inContext->GBuffer0Texture, viewDesc);
    const RenderViewHandle GBuffer1Handle     = pass.CreateView(inContext->GBuffer1Texture, viewDesc);
    const RenderViewHandle GBuffer2Handle     = pass.CreateView(inContext->GBuffer2Texture, viewDesc);
    const RenderViewHandle depthHandle        = pass.CreateView(inContext->depthTexture, viewDesc);

    viewDesc.state                            = kGPUResourceState_ComputeShaderWrite;
    const RenderViewHandle colourHandle       = pass.CreateView(inContext->colourTexture, viewDesc, &inContext->colourTexture);

    viewDesc.type                             = kGPUResourceViewType_Buffer;
    viewDesc.state                            = kGPUResourceState_ComputeShaderRead;
    viewDesc.elementCount                     = graph.GetBufferDesc(inContext->lightParamsBuffer).size;
    const RenderViewHandle paramsHandle       = pass.CreateView(inContext->lightParamsBuffer, viewDesc);

    viewDesc.elementCount                     = graph.GetBufferDesc(inContext->visibleLightCountBuffer).size;
    const RenderViewHandle visibleCountHandle = pass.CreateView(inContext->visibleLightCountBuffer, viewDesc);

    viewDesc.elementCount                     = graph.GetBufferDesc(inContext->visibleLightsBuffer).size;
    const RenderViewHandle visibleHandle      = pass.CreateView(inContext->visibleLightsBuffer, viewDesc);

    pass.SetFunction([=] (const RenderGraph&     inGraph,
                          const RenderGraphPass& inPass,
                          GPUComputeCommandList& inCmdList)
    {
        inCmdList.SetPipeline(mLightingPipeline.get());

        GPUArgument arguments[kDeferredLightingArgumentsCount];
        arguments[kDeferredLightingArguments_GBuffer0Texture].view   = inPass.GetView(GBuffer0Handle);
        arguments[kDeferredLightingArguments_GBuffer1Texture].view   = inPass.GetView(GBuffer1Handle);
        arguments[kDeferredLightingArguments_GBuffer2Texture].view   = inPass.GetView(GBuffer2Handle);
        arguments[kDeferredLightingArguments_DepthTexture].view      = inPass.GetView(depthHandle);
        arguments[kDeferredLightingArguments_LightParams].view       = inPass.GetView(paramsHandle);
        arguments[kDeferredLightingArguments_VisibleLightCount].view = inPass.GetView(visibleCountHandle);
        arguments[kDeferredLightingArguments_VisibleLights].view     = inPass.GetView(visibleHandle);
        arguments[kDeferredLightingArguments_ColourTexture].view     = inPass.GetView(colourHandle);

        inCmdList.SetArguments(kArgumentSet_DeferredLighting, arguments);

        DeferredLightingConstants constants;
        constants.tileDimensions = glm::uvec2(inContext->tilesWidth, inContext->tilesHeight);

        inCmdList.WriteConstants(kArgumentSet_DeferredLighting,
                                 kDeferredLightingArguments_Constants,
                                 &constants,
                                 sizeof(constants));

        inCmdList.SetConstants(kArgumentSet_ViewEntity,
                               kViewEntityArguments_ViewConstants,
                               inContext->GetView().GetConstants());

        inCmdList.Dispatch(inContext->tilesWidth, inContext->tilesHeight, 1);
    });
}

/**
 * Debug window implementation.
 */

DeferredRenderPipelineWindow::DeferredRenderPipelineWindow(DeferredRenderPipeline* const inPipeline) :
    DebugWindow ("Render", "Render Pipeline"),
    mPipeline   (inPipeline)
{
}

void DeferredRenderPipelineWindow::Render()
{
    ImGuiIO& io = ImGui::GetIO();
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
