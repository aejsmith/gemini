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

#include "Render/Camera.h"

#include "Engine/Window.h"

#include "Render/BasicRenderPipeline.h"
#include "Render/RenderGraph.h"
#include "Render/RenderLayer.h"

class CameraRenderLayer final : public RenderLayer
{
public:
                            CameraRenderLayer(Camera& inCamera);

protected:
     void                   AddPasses(RenderGraph&               inGraph,
                                      const RenderResourceHandle inTexture,
                                      RenderResourceHandle&      outNewTexture) override;

private:
    Camera&                 mCamera;

};

CameraRenderLayer::CameraRenderLayer(Camera& inCamera) :
    RenderLayer (RenderLayer::kOrder_World),
    mCamera     (inCamera)
{
}

void CameraRenderLayer::AddPasses(RenderGraph&               inGraph,
                                  const RenderResourceHandle inTexture,
                                  RenderResourceHandle&      outNewTexture)
{
    RenderGraphPass& pass = inGraph.AddPass("Clear", kRenderGraphPassType_Render);

    pass.SetColour(0, inTexture, &outNewTexture);
    pass.ClearColour(0, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    pass.SetFunction([] (const RenderGraph&      inGraph,
                         const RenderGraphPass&  inPass,
                         GPUGraphicsCommandList& inCmdList)
    {

    });
}

Camera::Camera() :
    renderPipeline  (new BasicRenderPipeline()),
    mRenderLayer    (new CameraRenderLayer(*this))
{
    mRenderLayer->SetLayerOutput(&MainWindow::Get());
}

Camera::~Camera()
{
    delete mRenderLayer;
}

void Camera::Activated()
{
    mRenderLayer->ActivateLayer();
}

void Camera::Deactivated()
{
    mRenderLayer->DeactivateLayer();
}

RenderOutput* Camera::GetOutput() const
{
    return mRenderLayer->GetLayerOutput();
}

void Camera::SetOutput(RenderOutput* const inOutput) const
{
    // TODO: This needs to be serialised somehow.
    mRenderLayer->SetLayerOutput(inOutput);
}
