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

#include "Core/HashTable.h"
#include "Core/Path.h"
#include "Core/Singleton.h"

#include "Engine/Asset.h"

#include <map>

/**
 * Manager for game assets. Assets are loaded from disk using this class. It
 * provides a virtual filesystem for accessing assets via a path string.
 * Strings starting with "Engine/" map to assets provided by the base engine,
 * while strings starting with "Game/" map to game-specific assets. Asset paths
 * do not have extensions: the type is known internally.
 * 
 * The way this works at the moment is somewhat temporary. At the moment we
 * always import asset data from source file types at runtime. In future, a
 * compiled game's assets would be binary serialised objects which include the
 * asset data. Loaders would become importers that initially create the assets
 * (e.g. in an editor), and wouldn't be included in a final game build.
 */
class AssetManager : public Singleton<AssetManager>
{
public:
                            AssetManager();
                            ~AssetManager();

    /**
     * Attempts to load the asset at the specified path. Returns null if the
     * asset could not be loaded.
     */
    AssetPtr                Load(const Path& inPath);

    /**
     * Loads the asset at the specified type, ensuring that it is the requested
     * type. Raises a fatal error if the asset could not be loaded.
     */
    template <typename T>
    ObjPtr<T>               Load(const Path& inPath);

    void                    UnregisterAsset(Asset* const inAsset,
                                            OnlyCalledBy<Asset>);

    /**
     * Get a filesystem path (without extension) corresponding to an asset
     * path. Returns false if the search path (first component) is unknown.
     */
    bool                    GetFilesystemPath(const Path& inPath,
                                              Path&       outFSPath);

    /**
     * Save an asset to a new asset path, i.e. serialise its current state to
     * be reloaded later. If the asset is currently unmanaged, it will be
     * managed after this call completes. If it is already managed, its path
     * will be updated to the new path after the call. Returns whether saving
     * was successful.
     */
    bool                    SaveAsset(Asset* const inAsset,
                                      const Path&  inPath);

private:
    using AssetMap        = std::map<std::string, Asset*>;
    using SearchPathMap   = HashMap<std::string, std::string>;

private:
    Asset*                  LookupAsset(const Path& inPath) const;

private:
    /**
     * Map of loaded assets. This stores a raw pointer rather than a reference
     * pointer since we don't want to increase the reference count. Assets
     * remove themselves from here when their reference count reaches 0.
     * 
     * TODO: Use a more appropriate data structure (radix tree?)
     */
    AssetMap                mAssets;

    SearchPathMap           mSearchPaths;

};

template <typename T>
inline ObjPtr<T> AssetManager::Load(const Path& inPath)
{
    static_assert(std::is_base_of<Asset, T>::value,
                  "AssetType is not derived from Asset");

    AssetPtr asset = Load(inPath);
    if (!asset)
    {
        Fatal("Unable to load asset '%s'", inPath.GetCString());
    }

    ObjPtr<T> ret = object_cast<ObjPtr<T>>(asset);
    if (!ret)
    {
        Fatal("Asset '%s' is not of expected type", inPath.GetCString());
    }

    return ret;
}
