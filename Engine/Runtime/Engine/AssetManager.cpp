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

#include "Engine/AssetManager.h"

#include "Core/Filesystem.h"
#include "Core/Platform.h"

#include "Engine/AssetLoader.h"
#include "Engine/DebugWindow.h"
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

AssetPtr AssetManager::Load(const Path& path)
{
    Asset* const exist = LookupAsset(path);
    if (exist)
    {
        return exist;
    }

    Path fsPath;
    if (!GetFilesystemPath(path, fsPath))
    {
        LogError("Could not find asset '%s' (unknown search path)", path.GetCString());
        return nullptr;
    }

    const Path directoryPath    = fsPath.GetDirectoryName();
    const std::string assetName = fsPath.GetBaseFileName();

    UPtr<Directory> directory(Filesystem::OpenDirectory(directoryPath));
    if (!directory)
    {
        LogError("Could not find asset '%s'", path.GetCString());
        return nullptr;
    }

    /* Iterate over directory entries to try to find the asset data and a
     * corresponding loader. */
    UPtr<DataStream> data;
    UPtr<DataStream> loaderData;
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
                    LogError("Asset '%s' has multiple data streams", path.GetCString());
                    return nullptr;
                }

                data.reset(Filesystem::OpenFile(filePath));
                if (!data)
                {
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
        LogError("Could not find asset '%s'", path.GetCString());
        return nullptr;
    }

    AssetPtr asset;

    /* Helper function used on both paths below to mark the asset as managed
     * and cache it. */
    auto AddAsset = [&] (Object* const object)
    {
        auto newAsset = static_cast<Asset*>(object);
        newAsset->SetPath(path.GetString(), {});
        mAssets.insert(std::make_pair(path.GetString(), newAsset));
    };

    if (type == kObjectFileExtension)
    {
        /* This is a serialised object. */
        type.clear();
        if (loaderData)
        {
            LogError("%s: Serialised object cannot have a loader", path.GetCString());
            return nullptr;
        }

        Assert(data);

        ByteArray serialisedData(data->GetSize());
        if (!data->Read(serialisedData.Get(), data->GetSize()))
        {
            LogError("%s: Failed to read asset data", path.GetCString());
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
            LogError("%s: Error during object deserialisation", path.GetCString());
            return nullptr;
        }
    }
    else
    {
        /* Get a loader for the asset. Use a serialised one if it exists, else
         * get a default one based on the file type. */
        ObjPtr<AssetLoader> loader;
        if (loaderData)
        {
            ByteArray serialisedData(loaderData->GetSize());
            if (!loaderData->Read(serialisedData.Get(), loaderData->GetSize()))
            {
                LogError("%s: Failed to read loader data", path.GetCString());
                return nullptr;
            }

            JSONSerialiser serialiser;
            loader = serialiser.Deserialise<AssetLoader>(serialisedData);
            if (!loader)
            {
                LogError("%s: Error during loader deserialisation", path.GetCString());
                return nullptr;
            }

            if (type != loader->GetExtension())
            {
                LogError("%s: Asset '%s' has loader but is for a different file type", path.GetCString());
                return nullptr;
            }
        }
        else
        {
            loader = AssetLoader::Create(type);
            if (!loader)
            {
                LogError("%s: Unknown file type '%s'", path.GetCString(), type.c_str());
                return nullptr;
            }
        }

        /* Create the asset. The loader should log an error if it fails. */
        asset = loader->Load(data.get(), path.GetCString());
        if (!asset)
        {
            return nullptr;
        }

        AddAsset(asset);
    }

    if (!type.empty())
    {
        LogDebug("Loaded asset '%s' from source file type '%s'", path.GetCString(), type.c_str());
    }
    else
    {
        LogDebug("Loaded asset '%s'", path.GetCString());
    }

    return asset;
}

void AssetManager::UnregisterAsset(Asset* const asset,
                                   OnlyCalledBy<Asset>)
{
    const size_t ret = mAssets.erase(asset->GetPath());
    AssertMsg(ret > 0,
              "Destroying asset '%s' which is not in the cache",
              asset->GetPath().c_str());
    Unused(ret);

    LogDebug("Unregistered asset '%s'", asset->GetPath().c_str());
}

bool AssetManager::GetFilesystemPath(const Path& path,
                                     Path&       outFSPath)
{
    auto searchPath = mSearchPaths.find(path.Subset(0, 1).GetString());
    if (searchPath == mSearchPaths.end())
    {
        return false;
    }

    outFSPath = Path(searchPath->second) / path.Subset(1);
    return true;
}

bool AssetManager::SaveAsset(Asset* const asset,
                             const Path&  path)
{
    Path fsPath;
    if (!GetFilesystemPath(path, fsPath))
    {
        LogError("Could not save asset '%s': unknown search path", path.GetCString());
        return false;
    }

    fsPath += ".";
    fsPath += kObjectFileExtension;

    JSONSerialiser serialiser;
    ByteArray serialisedData = serialiser.Serialise(asset);

    UPtr<File> file(Filesystem::OpenFile(fsPath, kFileMode_Write | kFileMode_Create | kFileMode_Truncate));
    if (!file)
    {
        LogError("Could not save asset '%s': failed to open '%s'", path.GetCString(), fsPath.GetCString());
        return false;
    }

    if (!file->Write(serialisedData.Get(), serialisedData.GetSize()))
    {
        LogError("Could not save asset '%s': failed to write", path.GetCString());
        return false;
    }

    LogDebug("Saved asset '%s' ('%s')", path.GetCString(), fsPath.GetCString());

    if (asset->IsManaged())
    {
        /* Re-insert under new path. */
        mAssets.erase(asset->GetPath());
    }

    asset->SetPath(path.GetString(), {});
    mAssets.insert(std::make_pair(path.GetString(), asset));

    return true;
}

Asset* AssetManager::LookupAsset(const Path& path) const
{
    auto it = mAssets.find(path.GetString());
    return (it != mAssets.end()) ? it->second : nullptr;
}

bool AssetManager::DebugUIAssetSelector(AssetPtr&        ioAsset,
                                        const MetaClass& pointeeClass,
                                        const bool       activate)
{
    /* Because this is a modal dialog, we should only have one active at a time
     * and so using a single static buffer is fine. */
    static char pathBuf[128] = {};

    if (activate)
    {
        ImGui::OpenPopup("Select Asset");

        if (ioAsset)
        {
            strncpy(pathBuf, ioAsset->GetPath().c_str(), ArraySize(pathBuf) - 1);
            pathBuf[ArraySize(pathBuf) - 1] = 0;
        }
        else
        {
            pathBuf[0] = 0;
        }
    }

    bool result = false;

    if (ImGui::BeginPopupModal("Select Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Asset path:");

        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::PushItemWidth(-1);
        const bool ok = ImGui::InputText("", &pathBuf[0], ArraySize(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();

        ImGui::Spacing();

        if (ok || ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();

            result = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    static const char* incorrectType = nullptr;

    if (result)
    {
        AssetPtr newAsset = Load(pathBuf);
        if (!newAsset)
        {
            result        = false;
            incorrectType = nullptr;

            ImGui::OpenPopup("Invalid Asset");
        }
        else if (!pointeeClass.IsBaseOf(newAsset->GetMetaClass()))
        {
            result        = false;
            incorrectType = newAsset->GetMetaClass().GetName();
            strncpy(pathBuf, ioAsset->GetPath().c_str(), ArraySize(pathBuf) - 1);

            ImGui::OpenPopup("Invalid Asset");
        }
        else
        {
            ioAsset = std::move(newAsset);
        }
    }

    if (ImGui::BeginPopupModal("Invalid Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (incorrectType)
        {
            ImGui::Text("Asset '%s' is incorrect type '%s'", pathBuf, incorrectType);
        }
        else
        {
            ImGui::Text("Asset '%s' could not be found", pathBuf);
        }

        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(-1, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return result;
}
