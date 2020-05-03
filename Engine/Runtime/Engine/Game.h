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

#include "Core/Singleton.h"

#include "Engine/Object.h"

/**
 * This is the class that is responsible for setting up the game once the engine
 * has been initialised. Game code must define a single class which is derived
 * from this. It will be looked up by the engine and an instance of it will be
 * constructed early in initialisation. Once the engine is initialised, the
 * Init() method will be called to set the game up.
 */
class Game : public Object,
             public Singleton<Game>
{
    CLASS();

public:
    virtual void                Init() = 0;

    /**
     * Return a name string for the game. This should be a short name without
     * spaces. It is used for game-specific filesystem paths (e.g. user settings
     * folder name).
     */
    virtual const char*         GetName() const = 0;

    /** Return a full title string for the game, used for display purposes. */
    virtual const char*         GetTitle() const = 0;

protected:
                                Game() {}
                                ~Game() {}

};
