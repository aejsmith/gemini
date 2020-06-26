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

#pragma once

#include "Core/Math/BoundingBox.h"
#include "Core/Math/Cone.h"
#include "Core/Math/Frustum.h"
#include "Core/Math/Line.h"
#include "Core/Math/Sphere.h"
#include "Core/Singleton.h"
#include "Core/String.h"
#include "Core/Utility.h"

#include "GPU/GPUArgumentSet.h"
#include "GPU/GPUShader.h"
#include "GPU/GPUState.h"

#include <list>
#include <map>
#include <mutex>
#include <vector>

class DebugInputHandler;
class DebugWindow;
class Engine;
class RenderGraph;
class RenderView;

struct RenderResourceHandle;

/** Debug drawing/HUD API. */
class DebugManager : public Singleton<DebugManager>
{
public:
                                DebugManager();

public:
    /**
     * Debug UI overlay.
     */

    void                        BeginFrame(OnlyCalledBy<Engine>);
    void                        RenderOverlay(OnlyCalledBy<Engine>);

    /** Display a line of debug text in the overlay. */
    void                        AddText(const char* const text,
                                        const glm::vec4&  colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    void                        AddText(const std::string& text,
                                        const glm::vec4&   colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
                                    { AddText(text.c_str(), colour); }

    void                        RegisterWindow(DebugWindow* const window,
                                               OnlyCalledBy<DebugWindow>);
    void                        UnregisterWindow(DebugWindow* const window,
                                                 OnlyCalledBy<DebugWindow>);

    void                        AddOverlayMenu(const char* const     name,
                                               std::function<void()> function);

    bool                        IsOverlayVisible() const
                                    { return mOverlayState >= kOverlayState_Visible; }

    /**
     * World-space debug drawing API.
     */

    void                        RenderPrimitives(const RenderView&     view,
                                                 RenderGraph&          graph,
                                                 RenderResourceHandle& ioDestTexture);

    void                        DrawPrimitive(const BoundingBox& box,
                                              const glm::vec4&   colour,
                                              const bool         fill = false);
    void                        DrawPrimitive(const Frustum&     frustum,
                                              const glm::vec4&   colour,
                                              const bool         fill = false);
    void                        DrawPrimitive(const Cone&        cone,
                                              const glm::vec4&   colour,
                                              const bool         fill = false);
    void                        DrawPrimitive(const Line&        line,
                                              const glm::vec4&   colour);
    void                        DrawPrimitive(const Sphere&      sphere,
                                              const glm::vec4&   colour,
                                              const bool         fill = false);

private:
    enum OverlayState
    {
        kOverlayState_Inactive,
        kOverlayState_Visible,
        kOverlayState_Active,
    };

    enum PrimitiveType
    {
        kPrimitiveType_BoundingBox,
        kPrimitiveType_Frustum,
        kPrimitiveType_Cone,
        kPrimitiveType_Line,
        kPrimitiveType_Sphere,
    };

    struct Primitive
    {
        union
        {
            BoundingBox         boundingBox;
            Frustum             frustum;
            Cone                cone;
            Line                line;
            Sphere              sphere;
        };

        PrimitiveType           type;
        glm::vec4               colour;
        bool                    fill;

    public:
                                Primitive()  {}
                                Primitive(const Primitive& other);
                                ~Primitive() {}

    };

    struct OverlayCategory
    {
        std::list<DebugWindow*> windows;
        std::function<void()>   menuFunction;
    };

    using OverlayCategoryMap  = std::map<std::string, OverlayCategory>;

private:
                                ~DebugManager();

private:
    OverlayCategoryMap          mOverlayCategories;
    DebugInputHandler*          mInputHandler;
    OverlayState                mOverlayState;

    GPUShaderPtr                mVertexShader;
    GPUShaderPtr                mPixelShader;
    GPUVertexInputStateRef      mVertexInputState;

    std::mutex                  mPrimitivesLock;
    std::vector<Primitive>      mPrimitives;

    friend class DebugInputHandler;
};
