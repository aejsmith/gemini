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

#include "Core/Math/BoundingBox.h"
#include "Core/Math/Cone.h"
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
    void                        AddText(const char* const inText,
                                        const glm::vec4&  inColour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    void                        AddText(const std::string& inText,
                                        const glm::vec4&   inColour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
                                    { AddText(inText.c_str(), inColour); }

    void                        RegisterWindow(DebugWindow* const inWindow,
                                               OnlyCalledBy<DebugWindow>);
    void                        UnregisterWindow(DebugWindow* const inWindow,
                                                 OnlyCalledBy<DebugWindow>);

    /**
     * World-space debug drawing API.
     */

    void                        RenderPrimitives(const RenderView&          inView,
                                                 RenderGraph&               inGraph,
                                                 const RenderResourceHandle inTexture,
                                                 RenderResourceHandle&      outNewTexture);

    void                        DrawPrimitive(const BoundingBox& inBox,
                                              const glm::vec3&   inColour);
    void                        DrawPrimitive(const Cone&        inCone,
                                              const glm::vec3&   inColour,
                                              const bool         inFill = false);
    void                        DrawPrimitive(const Line&        inLine,
                                              const glm::vec3&   inColour);
    void                        DrawPrimitive(const Sphere&      inSphere,
                                              const glm::vec3&   inColour,
                                              const bool         inFill = false);

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
        kPrimitiveType_Cone,
        kPrimitiveType_Line,
        kPrimitiveType_Sphere,
    };

    struct Primitive
    {
        union
        {
            BoundingBox         boundingBox;
            Cone                cone;
            Line                line;
            Sphere              sphere;
        };

        PrimitiveType           type;
        glm::vec3               colour;
        bool                    fill;

    public:
                                Primitive()  {}
                                Primitive(const Primitive& inOther);
                                ~Primitive() {}

    };

    using WindowList          = std::list<DebugWindow*>;
    using WindowCategoryMap   = std::map<std::string, WindowList>;

private:
                                ~DebugManager();

private:
    WindowCategoryMap           mWindows;
    DebugInputHandler*          mInputHandler;
    OverlayState                mOverlayState;

    GPUShaderPtr                mVertexShader;
    GPUShaderPtr                mPixelShader;
    GPURasterizerStateRef       mRasterizerState;
    GPURasterizerStateRef       mFillRasterizerState;
    GPUVertexInputStateRef      mVertexInputState;

    std::mutex                  mPrimitivesLock;
    std::vector<Primitive>      mPrimitives;

    friend class DebugInputHandler;
};
