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

#include "Loaders/TextureLoader.h"

/** Class implementing texture loading via stb_image. */
class STBLoader : public Texture2DLoader
{
    CLASS("constructable": false);

protected:
                                STBLoader() {}
                                ~STBLoader() {}

    bool                        LoadData() final override;

};

/* This doesn't do the whole class definition since libclang has an apparent
 * bug where if the class definition is from a macro, it won't count as being
 * in declared in the main file (meaning ObjectGen won't pick it up). */
#define STB_LOADER_CLASS_BODY(ClassName, Extension) \
    CLASS(); \
    public:    const char* GetExtension() const override { return Extension; } \
    protected: ~ClassName() {}

class TGALoader final : public STBLoader { STB_LOADER_CLASS_BODY(TGALoader, "tga"); };
class JPGLoader final : public STBLoader { STB_LOADER_CLASS_BODY(JPGLoader, "jpg"); };
class PNGLoader final : public STBLoader { STB_LOADER_CLASS_BODY(PNGLoader, "png"); };

#undef STB_LOADER_CLASS_BODY
