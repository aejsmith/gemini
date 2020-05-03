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

#include "Core/Path.h"
#include "Core/Singleton.h"

#include "Engine/Object.h"

class EngineSettings;
class World;

/**
 * Main class of the engine.
 */
class Engine : public Singleton<Engine>
{
public:
                            Engine();
                            ~Engine();

public:
    void                    Run();

    EngineSettings*         GetSettings() const         { return mSettings; }

    /**
     * Per-frame state.
     */

    /** Get the frame index (value incremented by 1 every frame). */
    uint64_t                GetFrameIndex() const       { return mFrameIndex; }

    /** Get the frame start time, in nanoseconds. */
    uint64_t                GetFrameStartTime() const   { return mFrameStartTime; }

    /** Get the last frame time, in nanoseconds. */
    uint64_t                GetLastFrameTime() const    { return mLastFrameTime; }

    /** Get the time delta for the current frame, in seconds. */
    float                   GetDeltaTime() const        { return mDeltaTime; }

    /**
     * World state.
     */

    /** Get the current game world. */
    World*                  GetWorld() const            { return mWorld; }

    /** Create a new, empty world, replacing the current world (if any). */
    void                    CreateWorld();

    /** Load a world asset, replacing the current world (if any). */
    void                    LoadWorld(const Path& path);

private:
    void                    InitSDL();
    void                    InitSettings();

private:
    Path                    mUserDirectoryPath;
    ObjPtr<EngineSettings>  mSettings;

    uint64_t                mFrameIndex;
    uint64_t                mFrameStartTime;
    uint64_t                mLastFrameTime;
    float                   mDeltaTime;
    uint64_t                mFPSUpdateTime;
    uint32_t                mFramesSinceFPSUpdate;
    float                   mFPS;

    ObjPtr<World>           mWorld;

};
