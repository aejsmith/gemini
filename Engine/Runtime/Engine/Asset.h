/*
 * Copyright (C) 2018 Alex Smith
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

#include "Engine/Object.h"

class AssetManager;

/**
 * All game assets (textures, meshes, etc) derive from this class. Managed
 * assets are ones which are stored on disk. These can be unloaded when they
 * are not needed and can be reloaded at a later time. Unmanaged assets are ones
 * created at runtime, these do not have any data on disk, and are lost when
 * they are destroyed.
 */
class Asset : public Object
{
    CLASS("constructable": false);

public:
    bool                    IsManaged() const   { return !mPath.empty(); }
    const std::string&      GetPath() const     { return mPath; }

    void                    SetPath(std::string inPath,
                                    OnlyCalledBy<AssetManager>);

protected:
                            Asset();
                            ~Asset();

    void                    Released() override;

private:
    std::string             mPath;

};

using AssetPtr = ObjectPtr<Asset>;
