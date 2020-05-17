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

#if !GEMINI_BUILD_RELEASE

/* MicroProfile includes Windows.h, use our wrapper to avoid some conflicts. */
#include "Core/Win32/Win32.h"

#define MICROPROFILE_IMPL 1
#define MICROPROFILEUI_IMPL 1

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wshadow"
#endif

#endif

#include "Engine/Profiler.h"

#include "Core/Thread.h"

#include "Engine/Engine.h"
#include "Engine/EngineSettings.h"
#include "Engine/DebugWindow.h"

#include "GPU/GPUContext.h"
#include "GPU/GPUDevice.h"
#include "GPU/GPUQueryPool.h"

#if GEMINI_PROFILER

#define MICROPROFILE_TEXT_WIDTH  6
#define MICROPROFILE_TEXT_HEIGHT 13

#include "microprofileui.h"

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

class ProfilerWindow : public DebugWindow
{
public:
                            ProfilerWindow();

protected:
    void                    Render() override;

};

SINGLETON_IMPL(Profiler);

static constexpr uint32_t kGPUFrameQueries = MICROPROFILE_GPU_MAX_QUERIES / MICROPROFILE_GPU_FRAMES;

static ImDrawList* gProfilerDrawList;
static glm::vec2 gProfilerDrawPosition;
static glm::vec2 gProfilerDrawSize;

Profiler::Profiler() :
    mGPUFrame     (0),
    mGPUFramePut  (0),
    mGPUSubmitted {},
    mGPUResults   {}
{
    MicroProfileInit();
    MicroProfileSetEnableAllGroups(true);
    MicroProfileOnThreadCreate("Main");

    g_MicroProfile.fReferenceTime = 16.66f;

    if (Engine::Get().GetSettings().profilerWebServer)
    {
        MicroProfileWebServerStart();
    }
}

Profiler::~Profiler()
{
}

void Profiler::GPUInit(OnlyCalledBy<Engine>)
{
    g_MicroProfile.GPU.Shutdown          = GPUShutdown;
    g_MicroProfile.GPU.Flip              = GPUFlip;
    g_MicroProfile.GPU.InsertTimer       = GPUInsertTimer;
    g_MicroProfile.GPU.GetTimeStamp      = GPUGetTimestamp;
    g_MicroProfile.GPU.GetTicksPerSecond = GPUTicksPerSecond;
    g_MicroProfile.GPU.GetTickReference  = GPUTickReference;

    GPUQueryPoolDesc desc;
    desc.type  = kGPUQueryType_Timestamp;
    desc.count = MICROPROFILE_GPU_MAX_QUERIES;

    mGPUQueryPool.reset(GPUDevice::Get().CreateQueryPool(desc));
}

void Profiler::WindowInit(OnlyCalledBy<Engine>)
{
    mWindow.reset(new ProfilerWindow);

    MicroProfileInitUI();
    MicroProfileSetDisplayMode(MP_DRAW_BARS);

    g_MicroProfileUI.nOpacityBackground = 64 << 24;
}

void Profiler::EndFrame(OnlyCalledBy<Engine>)
{
    MicroProfileFlip();
}

void Profiler::GPUShutdown()
{
}

uint32_t Profiler::GPUFlip()
{
    Profiler& self = Get();

    const uint32_t frameIndex     = self.mGPUFrame % MICROPROFILE_GPU_FRAMES;
    const uint32_t frameTimestamp = GPUInsertTimer(&GPUGraphicsContext::Get());
    const uint32_t framePut       = std::min(self.mGPUFramePut.load(), kGPUFrameQueries);

    self.mGPUSubmitted[frameIndex] = framePut;
    self.mGPUFramePut.store(0);
    self.mGPUFrame++;

    if (self.mGPUFrame >= MICROPROFILE_GPU_FRAMES)
    {
        const uint64_t pendingFrame      = self.mGPUFrame - MICROPROFILE_GPU_FRAMES;
        const uint32_t pendingFrameIndex = pendingFrame % MICROPROFILE_GPU_FRAMES;
        const uint32_t pendingFrameStart = pendingFrameIndex * kGPUFrameQueries;
        const uint32_t pendingFrameCount = self.mGPUSubmitted[pendingFrameIndex];

        if (pendingFrameCount)
        {
            self.mGPUQueryPool->GetResults(pendingFrameStart,
                                           pendingFrameCount,
                                           GPUQueryPool::kGetResults_Wait | GPUQueryPool::kGetResults_Reset,
                                           &self.mGPUResults[pendingFrameStart]);
        }
    }

    return frameTimestamp;
}

uint32_t Profiler::GPUInsertTimer(void* const context)
{
    Profiler& self = Get();

    // TODO: Multithreading support, support for command lists.
    Assert(Thread::IsMain());
    Assert(context);

    const uint32_t index = self.mGPUFramePut.fetch_add(1);

    if (index >= kGPUFrameQueries)
    {
        return static_cast<uint32_t>(-1);
    }

    const uint32_t queryIndex = ((self.mGPUFrame % MICROPROFILE_GPU_FRAMES) * kGPUFrameQueries) + index;

    auto gpuContext = static_cast<GPUTransferContext*>(context);
    gpuContext->Query(self.mGPUQueryPool.get(), queryIndex);

    return queryIndex;
}

uint64_t Profiler::GPUGetTimestamp(const uint32_t index)
{
    Profiler& self = Get();
    return self.mGPUResults[index];
}

uint64_t Profiler::GPUTicksPerSecond()
{
    /* GPU backend always uses nanoseconds. */
    return 1000000000ull;
}

bool Profiler::GPUTickReference(int64_t* outCPU, int64_t* outGPU)
{
    /* MicroProfile doesn't call this at the moment. */
    return false;
}

ProfilerWindow::ProfilerWindow() :
    DebugWindow ("Engine", "Profiler")
{
}

void ProfilerWindow::Render()
{
    ImGui::SetNextWindowPos(ImVec2(10, 110), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(715, 430), ImGuiCond_Once);

    if (!Begin())
    {
        return;
    }

    gProfilerDrawList = ImGui::GetWindowDrawList();

    const glm::vec2 windowSize    = ImGui::GetWindowSize();
    const glm::vec2 windowPos     = ImGui::GetWindowPos();
    const glm::vec2 cursorPos     = ImGui::GetCursorPos();
    const glm::vec2 mousePos      = ImGui::GetMousePos();
    const glm::vec2 localMousePos = mousePos - gProfilerDrawPosition;
    gProfilerDrawSize             = windowSize - (cursorPos * glm::vec2(2.0f, 1.2f));
    gProfilerDrawPosition         = cursorPos + windowPos;

    if (localMousePos.x >= 0 &&
        localMousePos.x < gProfilerDrawSize.x &&
        localMousePos.y >= 0 &&
        localMousePos.y < gProfilerDrawSize.y)
    {
        MicroProfileMousePosition(localMousePos.x,
                                  localMousePos.y,
                                  std::ceil(ImGui::GetIO().MouseWheel * 4.0f));

        MicroProfileMouseButton(ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                ImGui::IsMouseDown(ImGuiMouseButton_Right));
    }
    else
    {
        MicroProfileMousePosition(UINT32_MAX, UINT32_MAX, 0);
        MicroProfileMouseButton(0, 0);
    }

    /* MicroProfile's internal group clutters the UI, hide it. */
    uint16_t mpGroupIndex = MicroProfileGetGroup("MicroProfile", MicroProfileTokenTypeCpu);
    g_MicroProfile.nGroupMask &= ~(1ll << mpGroupIndex);

    MicroProfileDraw(gProfilerDrawSize.x, gProfilerDrawSize.y);

    g_MicroProfile.nGroupMask |= 1ll << mpGroupIndex;

    ImGui::End();
}

void MicroProfileDrawText(int nX, int nY, uint32_t nColor, const char* pText, uint32_t nNumCharacters)
{
    uint32_t a = 255;//0xff & (nColor >> 24);
    uint32_t r = 0xff & (nColor >> 16);
    uint32_t g = 0xff & (nColor >> 8);
    uint32_t b = 0xff & nColor;

    gProfilerDrawList->AddText(gProfilerDrawPosition + glm::vec2(nX, nY),
                               IM_COL32(r, g, b, a),
                               pText,
                               pText + nNumCharacters);
}

void MicroProfileDrawBox(int nX, int nY, int nX1, int nY1, uint32_t nColor, MicroProfileBoxType boxType)
{
    if (nX1 < 0 || nY1 < 0)
    {
        return;
    }

    nX  = std::max(nX, 0);
    nY  = std::max(nY, 0);
    nX1 = std::min(nX1, static_cast<int>(gProfilerDrawSize.x));
    nY1 = std::min(nY1, static_cast<int>(gProfilerDrawSize.y));

    uint32_t a = 0xff & (nColor >> 24);
    uint32_t r = 0xff & (nColor >> 16);
    uint32_t g = 0xff & (nColor >> 8);
    uint32_t b = 0xff & nColor;

    switch (boxType)
    {
        case MicroProfileBoxTypeBar:
        {
            uint32_t nMax = MicroProfileMax(MicroProfileMax(MicroProfileMax(r, g), b), 30u);
            uint32_t nMin = MicroProfileMin(MicroProfileMin(MicroProfileMin(r, g), b), 180u);

            uint32_t r0 = 0xff & ((r + nMax) / 2);
            uint32_t g0 = 0xff & ((g + nMax) / 2);
            uint32_t b0 = 0xff & ((b + nMax) / 2);

            uint32_t r1 = 0xff & ((r + nMin) / 2);
            uint32_t g1 = 0xff & ((g + nMin) / 2);
            uint32_t b1 = 0xff & ((b + nMin) / 2);

            uint32_t nColor0 = IM_COL32(r0, g0, b0, a);
            uint32_t nColor1 = IM_COL32(r1, g1, b1, a);

            gProfilerDrawList->AddRectFilledMultiColor(gProfilerDrawPosition + glm::vec2(nX, nY),
                                                       gProfilerDrawPosition + glm::vec2(nX1, nY1),
                                                       nColor0,
                                                       nColor0,
                                                       nColor1,
                                                       nColor1);

            break;
        }

        case MicroProfileBoxTypeFlat:
        {
            gProfilerDrawList->AddRectFilled(gProfilerDrawPosition + glm::vec2(nX, nY),
                                             gProfilerDrawPosition + glm::vec2(nX1, nY1),
                                             IM_COL32(r, g, b, a));

            break;
        }

        default:
        {
            Unreachable();
        }
    }
}

void MicroProfileDrawLine2D(uint32_t nVertices, float* pVertices, uint32_t nColor)
{
    uint32_t a = 0xff & (nColor >> 24);
    uint32_t r = 0xff & (nColor >> 16);
    uint32_t g = 0xff & (nColor >> 8);
    uint32_t b = 0xff & nColor;

    for (uint32_t vert = 0; vert < nVertices - 1; vert++)
    {
        uint32_t i = vert * 2;

        gProfilerDrawList->AddLine(gProfilerDrawPosition + glm::vec2(pVertices[i], pVertices[i + 1]),
                                   gProfilerDrawPosition + glm::vec2(pVertices[i + 2], pVertices[i + 3]),
                                   IM_COL32(r, g, b, a));
    }
}

#endif
