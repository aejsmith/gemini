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
                                DeferredRenderPipelineWindow(DeferredRenderPipeline* const pipeline);

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

    mDebugEntityBoundingBoxes   (false),
    mDebugLightVolumes          (false),
    mDebugLightCulling          (false),
    mDebugLightCullingMaximum   (20),

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

    /* Culling debug shader. */
    {
        mCullingDebugVertexShader = ShaderManager::Get().GetShader("Engine/DeferredCullingDebug.hlsl", "VSFullScreen", kGPUShaderStage_Vertex);
        mCullingDebugPixelShader  = ShaderManager::Get().GetShader("Engine/DeferredCullingDebug.hlsl", "PSMain", kGPUShaderStage_Pixel);

        GPUArgumentSetLayoutDesc argumentLayoutDesc(kDeferredCullingDebugArgumentsCount);
        argumentLayoutDesc.arguments[kDeferredCullingDebugArguments_VisibleLightCount] = kGPUArgumentType_Buffer;
        argumentLayoutDesc.arguments[kDeferredCullingDebugArguments_Constants]         = kGPUArgumentType_Constants;

        mCullingDebugArgumentSetLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));
    }
}

void DeferredRenderPipeline::SetName(std::string name)
{
    RenderPipeline::SetName(std::move(name));

    mDebugWindow->SetTitle(StringUtils::Format("Render Pipeline '%s'", GetName().c_str()));
}

void DeferredRenderPipeline::Render(const RenderWorld&         world,
                                    const RenderView&          view,
                                    RenderGraph&               graph,
                                    const RenderResourceHandle texture,
                                    RenderResourceHandle&      outNewTexture)
{
    DeferredRenderContext* const context = graph.NewTransient<DeferredRenderContext>(graph, world, view);

    /* Get the visible entities and lights. */
    context->GetWorld().Cull(context->GetView(), context->cullResults);

    CreateResources(context, texture);
    BuildDrawLists(context);
    PrepareLights(context);
    AddGBufferPasses(context);
    AddCullingPass(context);
    AddLightingPass(context);

    /* Tonemap and gamma correct onto the output texture. */
    mTonemapPass->AddPass(graph,
                          context->colourTexture,
                          texture,
                          outNewTexture);

    if (mDebugLightCulling)
    {
        AddCullingDebugPass(context, outNewTexture);
    }

    /* Render debug primitives for the view. */
    DebugManager::Get().RenderPrimitives(view,
                                         graph,
                                         outNewTexture,
                                         outNewTexture);
}

void DeferredRenderPipeline::CreateResources(DeferredRenderContext* const context,
                                             const RenderResourceHandle   outputTexture) const
{
    RenderGraph& graph = context->GetGraph();

    const RenderTextureDesc& outputDesc = graph.GetTextureDesc(outputTexture);

    /* Calculate output dimensions in number of tiles. */
    context->tilesWidth  = RoundUp(outputDesc.width,  static_cast<uint32_t>(kDeferredTileSize)) / kDeferredTileSize;
    context->tilesHeight = RoundUp(outputDesc.height, static_cast<uint32_t>(kDeferredTileSize)) / kDeferredTileSize;
    context->tilesCount  = context->tilesWidth * context->tilesHeight;

    RenderTextureDesc textureDesc;
    textureDesc.width                = outputDesc.width;
    textureDesc.height               = outputDesc.height;
    textureDesc.depth                = outputDesc.depth;

    textureDesc.name                 = "DeferredColour";
    textureDesc.format               = kColourFormat;
    context->colourTexture           = graph.CreateTexture(textureDesc);

    textureDesc.name                 = "DeferredDepth";
    textureDesc.format               = kDepthFormat;
    context->depthTexture            = graph.CreateTexture(textureDesc);

    textureDesc.name                 = "DeferredGBuffer0";
    textureDesc.format               = kGBuffer0Format;
    context->GBuffer0Texture         = graph.CreateTexture(textureDesc);

    textureDesc.name                 = "DeferredGBuffer1";
    textureDesc.format               = kGBuffer1Format;
    context->GBuffer1Texture         = graph.CreateTexture(textureDesc);

    textureDesc.name                 = "DeferredGBuffer2";
    textureDesc.format               = kGBuffer2Format;
    context->GBuffer2Texture         = graph.CreateTexture(textureDesc);

    RenderBufferDesc bufferDesc;

    bufferDesc.name                  = "DeferredLightParams";
    bufferDesc.size                  = sizeof(LightParams) * kDeferredMaxLightCount;
    context->lightParamsBuffer       = graph.CreateBuffer(bufferDesc);

    bufferDesc.name                  = "DeferredVisibleLightCount";
    bufferDesc.size                  = sizeof(uint32_t) * context->tilesCount;
    context->visibleLightCountBuffer = graph.CreateBuffer(bufferDesc);

    bufferDesc.name                  = "DeferredVisibleLights";
    bufferDesc.size                  = sizeof(uint32_t) * kDeferredVisibleLightsTileEntryCount * context->tilesCount;
    context->visibleLightsBuffer     = graph.CreateBuffer(bufferDesc);
}

void DeferredRenderPipeline::BuildDrawLists(DeferredRenderContext* const context) const
{
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

            if (mDebugEntityBoundingBoxes)
            {
                DebugManager::Get().DrawPrimitive(entity->GetWorldBoundingBox(), glm::vec3(0.0f, 0.0f, 1.0f));
            }
        }
    }

    context->opaqueDrawList.Sort();
}

void DeferredRenderPipeline::PrepareLights(DeferredRenderContext* const context) const
{
    auto& lightList = context->cullResults.lights;

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

        if (mDebugLightVolumes)
        {
            light->DrawDebugPrimitive();
        }
    }

    stagingBuffer.Finalise();

    context->GetGraph().AddUploadPass("DeferredLightParamsUpload",
                                      context->lightParamsBuffer,
                                      0,
                                      std::move(stagingBuffer),
                                      &context->lightParamsBuffer);
}

void DeferredRenderPipeline::AddGBufferPasses(DeferredRenderContext* const context) const
{
    RenderGraphPass& opaquePass = context->GetGraph().AddPass("DeferredOpaque", kRenderGraphPassType_Render);

    /* Colour output is bound as target 3 for emissive materials to output
     * directly to.
     *
     * TODO: We should mask output 3 in the pipeline state for non-emissive
     * materials. */
    opaquePass.SetColour(0, context->GBuffer0Texture, &context->GBuffer0Texture);
    opaquePass.SetColour(1, context->GBuffer1Texture, &context->GBuffer1Texture);
    opaquePass.SetColour(2, context->GBuffer2Texture, &context->GBuffer2Texture);
    opaquePass.SetColour(3, context->colourTexture,   &context->colourTexture);

    opaquePass.ClearColour(0, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearColour(1, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearColour(2, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    opaquePass.ClearColour(3, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    opaquePass.SetDepthStencil(context->depthTexture, kGPUResourceState_DepthStencilWrite, &context->depthTexture);

    opaquePass.ClearDepth(1.0f);

    context->opaqueDrawList.Draw(opaquePass);
}

void DeferredRenderPipeline::AddCullingPass(DeferredRenderContext* const context) const
{
    RenderGraph& graph = context->GetGraph();

    RenderGraphPass& pass = graph.AddPass("DeferredCulling", kRenderGraphPassType_Compute);

    RenderViewDesc viewDesc;

    viewDesc.type                             = kGPUResourceViewType_Texture2D;
    viewDesc.state                            = kGPUResourceState_ComputeShaderRead;
    const RenderViewHandle depthHandle        = pass.CreateView(context->depthTexture, viewDesc);

    viewDesc.type                             = kGPUResourceViewType_Buffer;
    viewDesc.elementCount                     = graph.GetBufferDesc(context->lightParamsBuffer).size;
    const RenderViewHandle paramsHandle       = pass.CreateView(context->lightParamsBuffer, viewDesc);

    viewDesc.state                            = kGPUResourceState_ComputeShaderWrite;
    viewDesc.elementCount                     = graph.GetBufferDesc(context->visibleLightCountBuffer).size;
    const RenderViewHandle visibleCountHandle = pass.CreateView(context->visibleLightCountBuffer, viewDesc, &context->visibleLightCountBuffer);

    viewDesc.elementCount                     = graph.GetBufferDesc(context->visibleLightsBuffer).size;
    const RenderViewHandle visibleHandle      = pass.CreateView(context->visibleLightsBuffer, viewDesc, &context->visibleLightsBuffer);

    pass.SetFunction([=] (const RenderGraph&     graph,
                          const RenderGraphPass& pass,
                          GPUComputeCommandList& cmdList)
    {
        cmdList.SetPipeline(mCullingPipeline.get());

        GPUArgument arguments[kDeferredCullingArgumentsCount];
        arguments[kDeferredCullingArguments_DepthTexture].view      = pass.GetView(depthHandle);
        arguments[kDeferredCullingArguments_LightParams].view       = pass.GetView(paramsHandle);
        arguments[kDeferredCullingArguments_VisibleLightCount].view = pass.GetView(visibleCountHandle);
        arguments[kDeferredCullingArguments_VisibleLights].view     = pass.GetView(visibleHandle);

        cmdList.SetArguments(kArgumentSet_DeferredCulling, arguments);

        DeferredCullingConstants constants;
        constants.tileDimensions = glm::uvec2(context->tilesWidth, context->tilesHeight);
        constants.lightCount     = context->cullResults.lights.size();

        cmdList.WriteConstants(kArgumentSet_DeferredCulling,
                               kDeferredCullingArguments_Constants,
                               &constants,
                               sizeof(constants));

        cmdList.SetConstants(kArgumentSet_ViewEntity,
                             kViewEntityArguments_ViewConstants,
                             context->GetView().GetConstants());

        cmdList.Dispatch(context->tilesWidth, context->tilesHeight, 1);
    });
}

void DeferredRenderPipeline::AddLightingPass(DeferredRenderContext* const context) const
{
    /* TODO: Investigate performance of compute vs pixel shader. Pixel may be
     * beneficial due to colour compression - AMD pre-Navi can't do compressed
     * UAV writes, same for at least NVIDIA Maxwell (unsure about anything
     * newer). However a pixel shader would probably need some tricks to
     * scalarize access to the tile/light data, as we have no guarantees on
     * whether or not pixel shader wavefronts can cross tile boundaries, and
     * the compiler would not be able to scalarize by itself. */

    RenderGraph& graph = context->GetGraph();

    RenderGraphPass& pass = graph.AddPass("DeferredLighting", kRenderGraphPassType_Compute);

    RenderViewDesc viewDesc;

    viewDesc.type                             = kGPUResourceViewType_Texture2D;
    viewDesc.state                            = kGPUResourceState_ComputeShaderRead;
    const RenderViewHandle GBuffer0Handle     = pass.CreateView(context->GBuffer0Texture, viewDesc);
    const RenderViewHandle GBuffer1Handle     = pass.CreateView(context->GBuffer1Texture, viewDesc);
    const RenderViewHandle GBuffer2Handle     = pass.CreateView(context->GBuffer2Texture, viewDesc);
    const RenderViewHandle depthHandle        = pass.CreateView(context->depthTexture, viewDesc);

    viewDesc.state                            = kGPUResourceState_ComputeShaderWrite;
    const RenderViewHandle colourHandle       = pass.CreateView(context->colourTexture, viewDesc, &context->colourTexture);

    viewDesc.type                             = kGPUResourceViewType_Buffer;
    viewDesc.state                            = kGPUResourceState_ComputeShaderRead;
    viewDesc.elementCount                     = graph.GetBufferDesc(context->lightParamsBuffer).size;
    const RenderViewHandle paramsHandle       = pass.CreateView(context->lightParamsBuffer, viewDesc);

    viewDesc.elementCount                     = graph.GetBufferDesc(context->visibleLightCountBuffer).size;
    const RenderViewHandle visibleCountHandle = pass.CreateView(context->visibleLightCountBuffer, viewDesc);

    viewDesc.elementCount                     = graph.GetBufferDesc(context->visibleLightsBuffer).size;
    const RenderViewHandle visibleHandle      = pass.CreateView(context->visibleLightsBuffer, viewDesc);

    pass.SetFunction([=] (const RenderGraph&     graph,
                          const RenderGraphPass& pass,
                          GPUComputeCommandList& cmdList)
    {
        cmdList.SetPipeline(mLightingPipeline.get());

        GPUArgument arguments[kDeferredLightingArgumentsCount];
        arguments[kDeferredLightingArguments_GBuffer0Texture].view   = pass.GetView(GBuffer0Handle);
        arguments[kDeferredLightingArguments_GBuffer1Texture].view   = pass.GetView(GBuffer1Handle);
        arguments[kDeferredLightingArguments_GBuffer2Texture].view   = pass.GetView(GBuffer2Handle);
        arguments[kDeferredLightingArguments_DepthTexture].view      = pass.GetView(depthHandle);
        arguments[kDeferredLightingArguments_LightParams].view       = pass.GetView(paramsHandle);
        arguments[kDeferredLightingArguments_VisibleLightCount].view = pass.GetView(visibleCountHandle);
        arguments[kDeferredLightingArguments_VisibleLights].view     = pass.GetView(visibleHandle);
        arguments[kDeferredLightingArguments_ColourTexture].view     = pass.GetView(colourHandle);

        cmdList.SetArguments(kArgumentSet_DeferredLighting, arguments);

        DeferredLightingConstants constants;
        constants.tileDimensions = glm::uvec2(context->tilesWidth, context->tilesHeight);

        cmdList.WriteConstants(kArgumentSet_DeferredLighting,
                               kDeferredLightingArguments_Constants,
                               &constants,
                               sizeof(constants));

        cmdList.SetConstants(kArgumentSet_ViewEntity,
                             kViewEntityArguments_ViewConstants,
                             context->GetView().GetConstants());

        cmdList.Dispatch(context->tilesWidth, context->tilesHeight, 1);
    });
}

void DeferredRenderPipeline::AddCullingDebugPass(DeferredRenderContext* const context,
                                                 RenderResourceHandle&        ioNewTexture) const
{
    RenderGraph& graph = context->GetGraph();

    RenderGraphPass& pass = graph.AddPass("DeferredCullingDebug", kRenderGraphPassType_Render);

    RenderViewDesc viewDesc;
    viewDesc.type                             = kGPUResourceViewType_Buffer;
    viewDesc.state                            = kGPUResourceState_PixelShaderRead;
    viewDesc.elementCount                     = graph.GetBufferDesc(context->visibleLightCountBuffer).size;
    const RenderViewHandle visibleCountHandle = pass.CreateView(context->visibleLightCountBuffer, viewDesc);

    pass.SetColour(0, ioNewTexture, &ioNewTexture);

    pass.SetFunction([=] (const RenderGraph&      graph,
                          const RenderGraphPass&  pass,
                          GPUGraphicsCommandList& cmdList)
    {
        /* Debug heatmap is blended over the main scene. */
        GPUBlendStateDesc blendState;
        blendState.attachments[0].enable          = true;
        blendState.attachments[0].srcColourFactor = kGPUBlendFactor_SrcAlpha;
        blendState.attachments[0].dstColourFactor = kGPUBlendFactor_OneMinusSrcAlpha;
        blendState.attachments[0].srcAlphaFactor  = kGPUBlendFactor_SrcAlpha;
        blendState.attachments[0].dstAlphaFactor  = kGPUBlendFactor_OneMinusSrcAlpha;

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex]                       = mCullingDebugVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]                        = mCullingDebugPixelShader;
        pipelineDesc.argumentSetLayouts[kArgumentSet_DeferredCullingDebug] = mCullingDebugArgumentSetLayout;
        pipelineDesc.blendState                                            = GPUBlendState::Get(blendState);
        pipelineDesc.depthStencilState                                     = GPUDepthStencilState::GetDefault();
        pipelineDesc.rasterizerState                                       = GPURasterizerState::GetDefault();
        pipelineDesc.renderTargetState                                     = cmdList.GetRenderTargetState();
        pipelineDesc.vertexInputState                                      = GPUVertexInputState::GetDefault();
        pipelineDesc.topology                                              = kGPUPrimitiveTopology_TriangleList;

        cmdList.SetPipeline(pipelineDesc);

        GPUArgument arguments[kDeferredCullingDebugArgumentsCount];
        arguments[kDeferredCullingDebugArguments_VisibleLightCount].view = pass.GetView(visibleCountHandle);

        cmdList.SetArguments(kArgumentSet_DeferredCullingDebug, arguments);

        DeferredCullingDebugConstants constants;
        constants.tileDimensions = glm::uvec2(context->tilesWidth, context->tilesHeight);
        constants.maxLightCount  = mDebugLightCullingMaximum;

        cmdList.WriteConstants(kArgumentSet_DeferredCullingDebug,
                               kDeferredCullingDebugArguments_Constants,
                               &constants,
                               sizeof(constants));

        cmdList.Draw(3);
    });
}

/**
 * Debug window implementation.
 */

DeferredRenderPipelineWindow::DeferredRenderPipelineWindow(DeferredRenderPipeline* const pipeline) :
    DebugWindow ("Render", "Render Pipeline"),
    mPipeline   (pipeline)
{
}

void DeferredRenderPipelineWindow::Render()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 430, 30), ImGuiCond_Once);

    if (!Begin(ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::Dummy(ImVec2(400, 0));

    if (ImGui::CollapsingHeader("Debug visualisation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Draw entity bounding boxes", &mPipeline->mDebugEntityBoundingBoxes);
        ImGui::Checkbox("Draw light volumes", &mPipeline->mDebugLightVolumes);
        ImGui::Checkbox("Visualise light culling", &mPipeline->mDebugLightCulling);

        if (mPipeline->mDebugLightCulling)
        {
            ImGui::InputInt("Heatmap max light count", &mPipeline->mDebugLightCullingMaximum, 1, 5);
        }
    }

    ImGui::End();
}
