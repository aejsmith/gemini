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

#include "Core/Math/Cone.h"

#include "Engine/DebugManager.h"
#include "Engine/DebugWindow.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUBuffer.h"
#include "GPU/GPUCommandList.h"
#include "GPU/GPUContext.h"
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

    struct ShadowLight
    {
        const RenderLight*      light;
        RenderView              view;
        EntityDrawList          drawList;
    };

    CullResults                 cullResults;
    EntityDrawList              opaqueDrawList;
    EntityDrawList              unlitDrawList;

    uint32_t                    tilesWidth;
    uint32_t                    tilesHeight;
    uint32_t                    tilesCount;

    std::vector<ShadowLight>    shadowLights;

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

    /** Shadow mask/maps. */
    RenderResourceHandle        shadowMaskTexture;
    RenderResourceHandle        shadowMapTextures[kLightTypeCount];
};

DeferredRenderPipeline::DeferredRenderPipeline() :
    shadowMapResolution         (512),
    maxShadowLights             (4),
    shadowBiasConstant          (0.0005f),

    mTonemapPass                (new TonemapPass),

    mDebugEntityBoundingBoxes   (false),
    mDebugLightVolumes          (false),
    mDebugLightCulling          (false),
    mDebugLightCullingMaximum   (20),

    mDebugWindow                (new DeferredRenderPipelineWindow(this))
{
    CreateShaders();
    CreatePersistentResources();
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
        argumentLayoutDesc.arguments[kDeferredLightingArguments_ShadowMaskTexture] = kGPUArgumentType_Texture;
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

    /* Shadow mask shaders. */
    {
        // TODO: Other light types.

        mShadowMaskVertexShader                  = ShaderManager::Get().GetShader("Engine/DeferredShadowMask.hlsl", "VSMain", kGPUShaderStage_Vertex);
        mShadowMaskPixelShaders[kLightType_Spot] = ShaderManager::Get().GetShader("Engine/DeferredShadowMask.hlsl", "PSSpotLight", kGPUShaderStage_Pixel);

        GPUArgumentSetLayoutDesc argumentLayoutDesc(kDeferredShadowMaskArgumentsCount);
        argumentLayoutDesc.arguments[kDeferredShadowMaskArguments_DepthTexture]     = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredShadowMaskArguments_ShadowMapTexture] = kGPUArgumentType_Texture;
        argumentLayoutDesc.arguments[kDeferredShadowMaskArguments_ShadowMapSampler] = kGPUArgumentType_Sampler;
        argumentLayoutDesc.arguments[kDeferredShadowMaskArguments_Constants]        = kGPUArgumentType_Constants;

        const GPUArgumentSetLayoutRef argumentLayout = GPUDevice::Get().GetArgumentSetLayout(std::move(argumentLayoutDesc));

        /*
         * For spot/point lights we want to render the back face of the light
         * volume geometry, so that it will still be rendered even if the view
         * is inside the light volume.
         *
         * Test for depth greater than or equal to the back face of the light
         * volume so that only pixels in front of it are touched. Additionally,
         * enable depth clamping so that the light volume is not clipped.
         *
         * TODO: Use depth bounds test if available to cull pixels that are
         * outside the light volume in front of it (depth test only culls ones
         * behind the volume).
         */
        GPUDepthStencilStateDesc depthDesc;
        depthDesc.depthTestEnable  = true;
        depthDesc.depthWriteEnable = false;
        depthDesc.depthCompareOp   = kGPUCompareOp_GreaterOrEqual;

        GPURasterizerStateDesc rasterizerDesc;
        rasterizerDesc.cullMode         = kGPUCullMode_Front;
        rasterizerDesc.depthClampEnable = true;

        GPURenderTargetStateDesc renderTargetDesc;
        renderTargetDesc.colour[0]    = kShadowMaskFormat;
        renderTargetDesc.depthStencil = kDepthFormat;

        GPUVertexInputStateDesc vertexDesc;
        vertexDesc.buffers[0].stride      = sizeof(glm::vec3);
        vertexDesc.attributes[0].semantic = kGPUAttributeSemantic_Position;
        vertexDesc.attributes[0].format   = kGPUAttributeFormat_R32G32B32_Float;

        GPUPipelineDesc pipelineDesc;
        pipelineDesc.shaders[kGPUShaderStage_Vertex] = mShadowMaskVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]  = mShadowMaskPixelShaders[kLightType_Spot];
        pipelineDesc.blendState                      = GPUBlendState::GetDefault();
        pipelineDesc.depthStencilState               = GPUDepthStencilState::Get(depthDesc);
        pipelineDesc.rasterizerState                 = GPURasterizerState::Get(rasterizerDesc);
        pipelineDesc.renderTargetState               = GPURenderTargetState::Get(renderTargetDesc);
        pipelineDesc.vertexInputState                = GPUVertexInputState::Get(vertexDesc);
        pipelineDesc.topology                        = kGPUPrimitiveTopology_TriangleList;

        pipelineDesc.argumentSetLayouts[kArgumentSet_ViewEntity]         = RenderManager::Get().GetViewEntityArgumentSetLayout();
        pipelineDesc.argumentSetLayouts[kArgumentSet_DeferredShadowMask] = argumentLayout;

        mShadowMaskPipelines[kLightType_Spot] = GPUDevice::Get().GetPipeline(pipelineDesc);
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

void DeferredRenderPipeline::CreatePersistentResources()
{
    GPUGraphicsContext& context = GPUGraphicsContext::Get();

    /* Cone light volume geometry. */
    {
        const Cone cone(glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 0.0f, -1.0f),
                        1.0f,
                        glm::pi<float>() / 4.0f);

        std::vector<glm::vec3> vertices;
        std::vector<uint16_t> indices;
        cone.CreateGeometry(20, vertices, indices);

        GPUBufferDesc bufferDesc;
        bufferDesc.usage = kGPUResourceUsage_Standard;
        bufferDesc.size  = vertices.size() * sizeof(vertices[0]);

        mConeVertexBuffer.reset(GPUDevice::Get().CreateBuffer(bufferDesc));

        GPUStagingBuffer stagingBuffer(kGPUStagingAccess_Write, bufferDesc.size);
        stagingBuffer.Write(vertices.data(), bufferDesc.size);
        stagingBuffer.Finalise();

        context.UploadBuffer(mConeVertexBuffer.get(), stagingBuffer, bufferDesc.size);
        context.ResourceBarrier(mConeVertexBuffer.get(), kGPUResourceState_TransferWrite, kGPUResourceState_VertexBufferRead);

        bufferDesc.size = indices.size() * sizeof(indices[0]);

        mConeIndexBuffer.reset(GPUDevice::Get().CreateBuffer(bufferDesc));

        stagingBuffer.Initialise(kGPUStagingAccess_Write, bufferDesc.size);
        stagingBuffer.Write(indices.data(), bufferDesc.size);
        stagingBuffer.Finalise();

        context.UploadBuffer(mConeIndexBuffer.get(), stagingBuffer, bufferDesc.size);
        context.ResourceBarrier(mConeIndexBuffer.get(), kGPUResourceState_TransferWrite, kGPUResourceState_IndexBufferRead);
    }

    /* Shadow map sampler. */
    {
        GPUSamplerDesc samplerDesc;
        samplerDesc.minFilter = kGPUFilter_Linear;
        samplerDesc.magFilter = kGPUFilter_Linear;
        samplerDesc.compareOp = kGPUCompareOp_Less;

        mShadowMapSampler = GPUDevice::Get().GetSampler(samplerDesc);
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
    RENDER_PROFILER_SCOPE("DeferredRenderPipeline");

    DeferredRenderContext* const context = graph.NewTransient<DeferredRenderContext>(graph, world, view);

    /* Get the visible entities and lights. */
    context->GetWorld().Cull(context->GetView(), kCullFlags_None, context->cullResults);

    CreateResources(context, texture);
    PrepareLights(context);
    BuildDrawLists(context);
    AddGBufferPasses(context);
    AddShadowPasses(context);
    AddCullingPass(context);
    AddLightingPass(context);
    AddUnlitPass(context);

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

void DeferredRenderPipeline::PrepareLights(DeferredRenderContext* const context) const
{
    RENDER_PROFILER_FUNC_SCOPE();

    RenderGraph& graph = context->GetGraph();

    auto& lightList = context->cullResults.lights;
    DebugManager::Get().AddText(StringUtils::Format("Visible Lights: %zu", lightList.size()));

    /* We have fixed size resources and light indices that can only cope with a
     * certain number of lights, ignore any lights that exceed this. */
    if (lightList.size() == 0)
    {
        return;
    }
    else if (lightList.size() > kDeferredMaxLightCount)
    {
        LogWarning("Visible light count %zu exceeds limit %zu, truncating list",
                   lightList.size(),
                   kDeferredMaxLightCount);

        lightList.resize(kDeferredMaxLightCount);
    }

    /* Fill a buffer with parameters for the visible lights. */
    GPUStagingBuffer stagingBuffer(kGPUStagingAccess_Write, sizeof(LightParams) * lightList.size());
    auto lightParams = stagingBuffer.MapWrite<LightParams>();

    context->shadowLights.reserve(this->maxShadowLights);

    for (size_t i = 0; i < lightList.size(); i++)
    {
        const RenderLight* const light = lightList[i];
        const LightType type           = light->GetType();

        light->GetLightParams(lightParams[i]);

        /* Prepare shadow state for shadow casting lights. */
        if (light->GetCastShadows())
        {
            if (context->shadowLights.size() < this->maxShadowLights)
            {
                lightParams[i].shadowMaskIndex = context->shadowLights.size();

                context->shadowLights.emplace_back();
                auto& shadowLight = context->shadowLights.back();

                shadowLight.light = light;

                if (!context->shadowMapTextures[type])
                {
                    context->shadowMapTextures[type] = CreateShadowMap(graph,
                                                                       type,
                                                                       this->shadowMapResolution);
                }

                CreateShadowView(light,
                                 this->shadowMapResolution,
                                 shadowLight.view);
            }
            else
            {
                LogWarning("Visible shadow casting light count exceeds limit %u, some shadows will be missing",
                           this->maxShadowLights);
            }
        }

        if (mDebugLightVolumes)
        {
            light->DrawDebugPrimitive();
        }
    }

    stagingBuffer.Finalise();

    graph.AddUploadPass("DeferredLightParamsUpload",
                        context->lightParamsBuffer,
                        0,
                        std::move(stagingBuffer),
                        &context->lightParamsBuffer);

    /* If we have any shadow casting lights, we need a shadow mask. */
    if (!context->shadowLights.empty())
    {
        const RenderTextureDesc& outputDesc = graph.GetTextureDesc(context->colourTexture);

        RenderTextureDesc maskDesc;
        maskDesc.name      = "DeferredShadowMask";
        maskDesc.type      = kGPUResourceType_Texture2D;
        maskDesc.format    = kShadowMaskFormat;
        maskDesc.width     = outputDesc.width;
        maskDesc.height    = outputDesc.height;
        maskDesc.arraySize = this->maxShadowLights;

        context->shadowMaskTexture = graph.CreateTexture(maskDesc);
    }
}

void DeferredRenderPipeline::BuildDrawLists(DeferredRenderContext* const context) const
{
    RENDER_PROFILER_FUNC_SCOPE();

    /* Build draw lists for the opaque and unlit passes. Don't bother pre-
     * reserving space for the unlit draw list since there won't be many things
     * in there. */
    context->opaqueDrawList.Reserve(context->cullResults.entities.size());

    for (const RenderEntity* entity : context->cullResults.entities)
    {
        if (entity->SupportsPassType(kShaderPassType_DeferredOpaque))
        {
            const GPUPipelineRef pipeline = entity->GetPipeline(kShaderPassType_DeferredOpaque);

            const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
            EntityDrawCall& drawCall = context->opaqueDrawList.Add(sortKey);

            entity->GetDrawCall(kShaderPassType_DeferredOpaque, context->GetView(), drawCall);
        }
        else if (entity->SupportsPassType(kShaderPassType_DeferredUnlit))
        {
            const GPUPipelineRef pipeline = entity->GetPipeline(kShaderPassType_DeferredUnlit);

            const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
            EntityDrawCall& drawCall = context->unlitDrawList.Add(sortKey);

            entity->GetDrawCall(kShaderPassType_DeferredUnlit, context->GetView(), drawCall);
        }

        if (mDebugEntityBoundingBoxes)
        {
            const BoundingBox& box = entity->GetWorldBoundingBox();

            if (box.GetMaximum() != glm::vec3(FLT_MAX))
            {
                DebugManager::Get().DrawPrimitive(box, glm::vec3(0.0f, 0.0f, 1.0f));
            }
        }
    }

    context->opaqueDrawList.Sort();
    context->unlitDrawList.Sort();

    const size_t entityCount = context->opaqueDrawList.Size() + context->unlitDrawList.Size();
    DebugManager::Get().AddText(StringUtils::Format("Visible Entities: %zu", entityCount));

    /* Build shadow map draw lists. */
    for (auto& shadowLight : context->shadowLights)
    {
        CullResults cullResults;
        context->GetWorld().Cull(shadowLight.view, kCullFlags_NoLights, cullResults);

        shadowLight.drawList.Reserve(cullResults.entities.size());

        for (const RenderEntity* entity : cullResults.entities)
        {
            if (entity->SupportsPassType(kShaderPassType_ShadowMap))
            {
                const GPUPipelineRef pipeline = entity->GetPipeline(kShaderPassType_ShadowMap);

                // TODO: Sort based on depth instead.
                const EntityDrawSortKey sortKey = EntityDrawSortKey::GetOpaque(pipeline);
                EntityDrawCall& drawCall = shadowLight.drawList.Add(sortKey);

                entity->GetDrawCall(kShaderPassType_ShadowMap, shadowLight.view, drawCall);
            }
        }

        shadowLight.drawList.Sort();
    }
}

void DeferredRenderPipeline::AddGBufferPasses(DeferredRenderContext* const context) const
{
    /* Pass is added even if the draw list is empty to clear the targets. */
    RenderGraphPass& pass = context->GetGraph().AddPass("DeferredOpaque", kRenderGraphPassType_Render);

    /*
     * Colour output is bound as target 3 for emissive materials to output
     * directly to.
     *
     * TODO: We should mask output 3 in the pipeline state for non-emissive
     * materials.
     */
    pass.SetColour(0, context->GBuffer0Texture, &context->GBuffer0Texture);
    pass.SetColour(1, context->GBuffer1Texture, &context->GBuffer1Texture);
    pass.SetColour(2, context->GBuffer2Texture, &context->GBuffer2Texture);
    pass.SetColour(3, context->colourTexture,   &context->colourTexture);

    pass.ClearColour(0, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    pass.ClearColour(1, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    pass.ClearColour(2, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    pass.ClearColour(3, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    pass.SetDepthStencil(context->depthTexture, kGPUResourceState_DepthStencilWrite, &context->depthTexture);

    pass.ClearDepth(1.0f);

    context->opaqueDrawList.Draw(pass);
}

void DeferredRenderPipeline::AddUnlitPass(DeferredRenderContext* const context) const
{
    if (!context->unlitDrawList.IsEmpty())
    {
        RenderGraphPass& pass = context->GetGraph().AddPass("DeferredUnlit", kRenderGraphPassType_Render);

        pass.SetColour(0, context->colourTexture, &context->colourTexture);

        pass.SetDepthStencil(context->depthTexture, kGPUResourceState_DepthStencilWrite, &context->depthTexture);

        context->unlitDrawList.Draw(pass);
    }
}

void DeferredRenderPipeline::DrawLightVolume(DeferredRenderContext* const context,
                                             const RenderLight* const     light,
                                             GPUGraphicsCommandList&      cmdList) const
{
    Transform transform;
    bool isIndexed = false;
    uint32_t count;

    switch (light->GetType())
    {
        case kLightType_Spot:
        {
            isIndexed = true;
            count     = mConeIndexBuffer->GetSize() / sizeof(uint16_t);

            cmdList.SetVertexBuffer(0, mConeVertexBuffer.get());
            cmdList.SetIndexBuffer(kGPUIndexType_16, mConeIndexBuffer.get());

            /* Transform our unit cone geometry to match the light. */
            const float radius = light->GetRange() * tanf(light->GetConeAngle());
            transform.Set(light->GetPosition(),
                          glm::rotation(glm::vec3(0.0f, 0.0f, -1.0f), light->GetDirection()),
                          glm::vec3(radius, radius, light->GetRange()));

            break;
        }

        default:
        {
            Fatal("TODO: Point/Directional shadows");
        }
    }

    EntityConstants entityConstants;
    entityConstants.transform = transform.GetMatrix();
    entityConstants.position  = transform.GetPosition();

    cmdList.WriteConstants(kArgumentSet_ViewEntity,
                           kViewEntityArguments_EntityConstants,
                           entityConstants);

    cmdList.SetConstants(kArgumentSet_ViewEntity,
                         kViewEntityArguments_ViewConstants,
                         context->GetView().GetConstants());

    if (isIndexed)
    {
        cmdList.DrawIndexed(count);
    }
    else
    {
        cmdList.Draw(count);
    }
}

void DeferredRenderPipeline::AddShadowPasses(DeferredRenderContext* const context) const
{
    RenderGraph& graph = context->GetGraph();

    for (size_t maskLayer = 0; maskLayer < context->shadowLights.size(); maskLayer++)
    {
        const auto& shadowLight = context->shadowLights[maskLayer];

        RenderResourceHandle& mapTexture = context->shadowMapTextures[shadowLight.light->GetType()];

        /* Render the shadow map. */
        {
            RenderGraphPass& mapPass = graph.AddPass(StringUtils::Format("ShadowMap_%zu", maskLayer), kRenderGraphPassType_Render);

            mapPass.SetDepthStencil(mapTexture, kGPUResourceState_DepthStencilWrite, &mapTexture);

            mapPass.ClearDepth(1.0f);

            shadowLight.drawList.Draw(mapPass);
        }

        /* Render the shadow map into the shadow mask. */
        {
            RenderGraphPass& maskPass = graph.AddPass(StringUtils::Format("DeferredShadowMask_%zu", maskLayer), kRenderGraphPassType_Render);

            RenderViewDesc viewDesc;
            viewDesc.type          = kGPUResourceViewType_Texture2D;
            viewDesc.state         = kGPUResourceState_RenderTarget;
            viewDesc.elementOffset = maskLayer;

            maskPass.SetColour(0,
                               context->shadowMaskTexture,
                               viewDesc,
                               &context->shadowMaskTexture);

            maskPass.ClearColour(0, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));

            /* We're not writing to depth here. Bind it read-only so that we
             * can also sample it in the shader. */
            maskPass.SetDepthStencil(context->depthTexture, kGPUResourceState_DepthStencilRead);

            viewDesc.state                     = kGPUResourceState_PixelShaderRead;
            viewDesc.elementOffset             = 0;
            const RenderViewHandle depthHandle = maskPass.CreateView(context->depthTexture, viewDesc);

            const RenderViewHandle mapHandle = maskPass.CreateView(mapTexture, viewDesc);

            maskPass.SetFunction([=] (const RenderGraph&      graph,
                                      const RenderGraphPass&  pass,
                                      GPUGraphicsCommandList& cmdList)
            {
                const auto& shadowLight  = context->shadowLights[maskLayer];

                cmdList.SetPipeline(mShadowMaskPipelines[shadowLight.light->GetType()]);

                GPUArgument arguments[kDeferredShadowMaskArgumentsCount];
                arguments[kDeferredShadowMaskArguments_DepthTexture].view        = pass.GetView(depthHandle);
                arguments[kDeferredShadowMaskArguments_ShadowMapTexture].view    = pass.GetView(mapHandle);
                arguments[kDeferredShadowMaskArguments_ShadowMapSampler].sampler = mShadowMapSampler;

                cmdList.SetArguments(kArgumentSet_DeferredShadowMask, arguments);

                DeferredShadowMaskConstants constants;
                constants.position     = shadowLight.light->GetPosition();
                constants.range        = shadowLight.light->GetRange();
                constants.direction    = shadowLight.light->GetDirection();
                constants.biasConstant = this->shadowBiasConstant;

                if (shadowLight.light->GetType() == kShaderLightType_Spot)
                {
                    constants.cosSpotAngle        = cosf(shadowLight.light->GetConeAngle());
                    constants.worldToShadowMatrix = shadowLight.view.GetViewProjectionMatrix();
                }

                cmdList.WriteConstants(kArgumentSet_DeferredShadowMask,
                                       kDeferredShadowMaskArguments_Constants,
                                       constants);

                DrawLightVolume(context, shadowLight.light, cmdList);
            });
        }
    }
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
                               constants);

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

    RenderViewHandle shadowMaskHandle;

    if (context->shadowMaskTexture)
    {
        viewDesc.type                         = kGPUResourceViewType_Texture2DArray;
        viewDesc.state                        = kGPUResourceState_ComputeShaderRead;
        viewDesc.elementCount                 = this->maxShadowLights;

        shadowMaskHandle                      = pass.CreateView(context->shadowMaskTexture, viewDesc);
    }

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

        arguments[kDeferredLightingArguments_ShadowMaskTexture].view =
            (context->shadowMaskTexture)
                ? pass.GetView(shadowMaskHandle)
                : RenderManager::Get().GetDummyWhiteTexture2DArray();

        cmdList.SetArguments(kArgumentSet_DeferredLighting, arguments);

        DeferredLightingConstants constants;
        constants.tileDimensions = glm::uvec2(context->tilesWidth, context->tilesHeight);

        cmdList.WriteConstants(kArgumentSet_DeferredLighting,
                               kDeferredLightingArguments_Constants,
                               constants);

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
        pipelineDesc.shaders[kGPUShaderStage_Vertex] = mCullingDebugVertexShader;
        pipelineDesc.shaders[kGPUShaderStage_Pixel]  = mCullingDebugPixelShader;
        pipelineDesc.blendState                      = GPUBlendState::Get(blendState);
        pipelineDesc.depthStencilState               = GPUDepthStencilState::GetDefault();
        pipelineDesc.rasterizerState                 = GPURasterizerState::GetDefault();
        pipelineDesc.renderTargetState               = cmdList.GetRenderTargetState();
        pipelineDesc.vertexInputState                = GPUVertexInputState::GetDefault();
        pipelineDesc.topology                        = kGPUPrimitiveTopology_TriangleList;

        pipelineDesc.argumentSetLayouts[kArgumentSet_DeferredCullingDebug] = mCullingDebugArgumentSetLayout;

        cmdList.SetPipeline(pipelineDesc);

        GPUArgument arguments[kDeferredCullingDebugArgumentsCount];
        arguments[kDeferredCullingDebugArguments_VisibleLightCount].view = pass.GetView(visibleCountHandle);

        cmdList.SetArguments(kArgumentSet_DeferredCullingDebug, arguments);

        DeferredCullingDebugConstants constants;
        constants.tileDimensions = glm::uvec2(context->tilesWidth, context->tilesHeight);
        constants.maxLightCount  = mDebugLightCullingMaximum;

        cmdList.WriteConstants(kArgumentSet_DeferredCullingDebug,
                               kDeferredCullingDebugArguments_Constants,
                               constants);

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
