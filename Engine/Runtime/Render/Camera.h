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

#pragma once

#include "Engine/Component.h"

#include "Render/RenderPipeline.h"

class CameraRenderLayer;
class RenderOutput;

/**
 * A camera implements a view into the world (from the position/orientation of
 * the entity that it is attached to) for rendering. The world as visible to
 * the camera is rendered to the specified output, using the configured render
 * pipeline.
 */
class Camera final : public Component
{
    CLASS();

public:
                                Camera();

    /** Render pipeline to use for the camera. */
    PROPERTY()
    ObjectPtr<RenderPipeline>   renderPipeline;

    /**
     * Output configuration.
     */

    RenderOutput*               GetOutput() const;
    void                        SetOutput(RenderOutput* const inOutput) const;

protected:
                                ~Camera();

    void                        Activated() override;
    void                        Deactivated() override;

private:
    CameraRenderLayer*          mRenderLayer;

};
