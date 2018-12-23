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

#include "Engine/AssetManager.h"

#include "Core/Filesystem.h"
#include "Core/Platform.h"

#include "Engine/AssetLoader.h"
#include "Engine/JSONSerialiser.h"

#include <memory>

static constexpr char kObjectFileExtension[] = "object";
static constexpr char kLoaderFileExtension[] = "loader";

SINGLETON_IMPL(AssetManager);

AssetManager::AssetManager()
{
    mSearchPaths.emplace("Engine", "Engine/Assets");

    const std::string gamePath = StringUtils::Format("Games/%s/Assets", Platform::GetProgramName().c_str());
    mSearchPaths.emplace("Game", gamePath);

    LogDebug("Asset search paths:");
    for (const auto& it : mSearchPaths)
    {
        LogDebug("  %-6s = %s", it.first.c_str(), it.second.c_str());
    }
}

AssetManager::~AssetManager()
{
    /* Assets should have been destroyed by now. */
    Assert(mAssets.empty());
}

AssetPtr AssetManager::Load(const Path& inPath)
{
    /* Look up the path in the cache of known assets. */
    Asset* const exist = LookupAsset(inPath);
    if (exist)
    {
        return exist;
    }

    /* Turn the asset path into a filesystem path. */
    auto searchPath = mSearchPaths.find(inPath.Subset(0, 1).GetString());
    if (searchPath == mSearchPaths.end())
    {
        LogError("Could not find asset '%s' (unknown search path)", inPath.GetCString());
        return nullptr;
    }

    const Path fsPath           = Path(searchPath->second) / inPath.Subset(1);
    const Path directoryPath    = fsPath.GetDirectoryName();
    const std::string assetName = fsPath.GetBaseFileName();

    std::unique_ptr<Directory> directory(Filesystem::OpenDirectory(directoryPath));
    if (!directory)
    {
        LogError("Could not find asset '%s'", inPath.GetCString());
        return nullptr;
    }

    /* Iterate over directory entries to try to find the asset data and a
     * corresponding loader. */
    std::unique_ptr<DataStream> data;
    std::unique_ptr<DataStream> loaderData;
    std::string type;
    Directory::Entry entry;
    while (directory->Next(entry))
    {
        if (entry.type != kFileType_File)
        {
            continue;
        }

        if (entry.name.GetBaseFileName() == assetName)
        {
            const std::string entryExt = entry.name.GetExtension();
            const Path filePath        = directoryPath / entry.name;

            if (entryExt == kLoaderFileExtension)
            {
                loaderData.reset(Filesystem::OpenFile(filePath));
                if (!loaderData)
                {
                    LogError("Failed to open '%s'", filePath.GetCString());
                    return nullptr;
                }
            }
            else if (!entryExt.empty())
            {
                if (data)
                {
                    LogError("Asset '%s' has multiple data streams", inPath.GetCString());
                    return nullptr;
                }

                data.reset(Filesystem::OpenFile(filePath));
                if (!data) {
                    LogError("Failed to open '%s'", filePath.GetCString());
                    return nullptr;
                }

                type = entryExt;
            }
        }
    }

    /* Succeeded if we have at least a data stream. */
    if (!data)
    {
        LogError("Could not find asset '%s'", inPath.GetCString());
        return nullptr;
    }

    AssetPtr asset;

    /* Helper function used on both paths below to mark the asset as managed
     * and cache it. */
    auto AddAsset =
        [&] (Object* const inObject)
        {
            auto asset = static_cast<Asset*>(inObject);
            asset->SetPath(inPath.GetString(), {});
            mAssets.insert(std::make_pair(inPath.GetString(), asset));
        };

    if (type == kObjectFileExtension)
    {
        /* This is a serialised object. */
        type.clear();
        if (loaderData)
        {
            LogError("%s: Serialised object cannot have a loader", inPath.GetCString());
            return nullptr;
        }

        Assert(data);

        std::vector<uint8_t> serialisedData(data->GetSize());
        if (!data->Read(&serialisedData[0], data->GetSize()))
        {
            LogError("%s: Failed to read asset data", inPath.GetCString());
            return nullptr;
        }

        JSONSerialiser serialiser;

        /* We make the asset managed prior to calling its Deserialise() method.
         * This is done for 2 reasons. Firstly, it makes the path available to
         * the Deserialise() method. Secondly, it means that any references
         * back to the asset by itself or child objects will correctly be
         * resolved to it, rather than causing a recursive attempt to load the
         * asset. */
        serialiser.postConstructFunction = AddAsset;

        asset = serialiser.Deserialise<Asset>(serialisedData);
        if (!asset)
        {
            LogError("%s: Error during object deserialisation", inPath.GetCString());
            return nullptr;
        }
    }
    else
    {
        /* Get a loader for the asset. Use a serialised one if it exists, else
         * get a default one based on the file type. */
        ObjectPtr<AssetLoader> loader;
        if (loaderData)
        {
            std::vector<uint8_t> serialisedData(loaderData->GetSize());
            if (!loaderData->Read(&serialisedData[0], loaderData->GetSize()))
            {
                LogError("%s: Failed to read loader data", inPath.GetCString());
                return nullptr;
            }

            JSONSerialiser serialiser;
            loader = serialiser.Deserialise<AssetLoader>(serialisedData);
            if (!loader)
            {
                LogError("%s: Error during loader deserialisation", inPath.GetCString());
                return nullptr;
            }

            if (type != loader->GetExtension())
            {
                LogError("%s: Asset '%s' has loader but is for a different file type", inPath.GetCString());
                return nullptr;
            }
        }
        else
        {
            loader = AssetLoader::Create(type);
            if (!loader)
            {
                LogError("%s: Unknown file type '%s'", inPath.GetCString(), type.c_str());
                return nullptr;
            }
        }

        /* Create the asset. The loader should log an error if it fails. */
        asset = loader->Load(data.get(), inPath.GetCString());
        if (!asset)
        {
            return nullptr;
        }

        AddAsset(asset);
    }

    if (!type.empty())
    {
        LogDebug("Loaded asset '%s' from source file type '%s'", inPath.GetCString(), type.c_str());
    }
    else
    {
        LogDebug("Loaded asset '%s'", inPath.GetCString());
    }

    return asset;
}

void AssetManager::UnregisterAsset(Asset* const inAsset,
                                   OnlyCalledBy<Asset>)
{
    const size_t ret = mAssets.erase(inAsset->GetPath());
    AssertMsg(ret > 0,
              "Destroying asset '%s' which is not in the cache",
              inAsset->GetPath().c_str());
    Unused(ret);

    LogDebug("Unregistered asset '%s'", inAsset->GetPath().c_str());
}

Asset* AssetManager::LookupAsset(const Path& inPath) const
{
    auto it = mAssets.find(inPath.GetString());
    return (it != mAssets.end()) ? it->second : nullptr;
}
