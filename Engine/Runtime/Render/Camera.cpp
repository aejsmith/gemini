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

#include "Entity/World.h"

#include "Render/DeferredRenderPipeline.h"
#include "Render/RenderGraph.h"
#include "Render/RenderLayer.h"
#include "Render/RenderView.h"

class CameraRenderLayer final : public RenderLayer
{
public:
                            CameraRenderLayer(const Camera& inCamera);

    std::string             GetName() const override;

protected:
     void                   AddPasses(RenderGraph&               inGraph,
                                      const RenderResourceHandle inTexture,
                                      RenderResourceHandle&      outNewTexture) override;

private:
    const Camera&           mCamera;

};

CameraRenderLayer::CameraRenderLayer(const Camera& inCamera) :
    RenderLayer (RenderLayer::kOrder_World),
    mCamera     (inCamera)
{
}

std::string CameraRenderLayer::GetName() const
{
    return std::string("Camera '") + mCamera.GetEntity()->GetPath() + std::string("'");
}

void CameraRenderLayer::AddPasses(RenderGraph&               inGraph,
                                  const RenderResourceHandle inTexture,
                                  RenderResourceHandle&      outNewTexture)
{
    Assert(mCamera.renderPipeline);

    const RenderView view =
        RenderView::CreatePerspective(mCamera.GetWorldPosition(),
                                      mCamera.GetWorldOrientation(),
                                      glm::radians(mCamera.verticalFOV),
                                      mCamera.zNear,
                                      mCamera.zFar,
                                      GetLayerOutput()->GetSize());

    mCamera.renderPipeline->Render(*mCamera.GetEntity()->GetWorld()->GetRenderWorld(),
                                   view,
                                   inGraph,
                                   inTexture,
                                   outNewTexture);
}

Camera::Camera() :
    renderPipeline  (new DeferredRenderPipeline()),
    verticalFOV     (60.0f),
    zNear           (0.1f),
    zFar            (500.0f),
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
    this->renderPipeline->SetName(GetEntity()->GetPath());

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
