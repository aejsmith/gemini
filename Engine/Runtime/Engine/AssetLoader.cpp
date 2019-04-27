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

#include "Engine/AssetLoader.h"

#include <map>

AssetLoader::AssetLoader() :
    mData   (nullptr),
    mPath   (nullptr)
{
}

AssetLoader::~AssetLoader()
{
}

AssetPtr AssetLoader::Load(DataStream* const inData,
                           const char* const inPath)
{
    mData = inData;
    mPath = inPath;

    return Load();
}

ObjectPtr<AssetLoader> AssetLoader::Create(const std::string& inExtension)
{
    /* Map of file types to loader class. */
    static auto sTypeMap =
        [] ()
        {
            std::map<std::string, const MetaClass*> map;

            MetaClass::Visit(
                [&] (const MetaClass& inMetaClass)
                {
                    if (AssetLoader::staticMetaClass.IsBaseOf(inMetaClass) && inMetaClass.IsConstructable())
                    {
                        ObjectPtr<Object> object      = inMetaClass.Construct();
                        ObjectPtr<AssetLoader> loader = object.StaticCast<AssetLoader>();

                        const char* const extension = loader->GetExtension();
                        
                        map.insert(std::make_pair(extension, &inMetaClass));
                    }
                });

            return map;
        }();

    ObjectPtr<AssetLoader> loader;

    auto ret = sTypeMap.find(inExtension);
    if (ret != sTypeMap.end())
    {
        ObjectPtr<Object> object = ret->second->Construct();
        loader = object.StaticCast<AssetLoader>();
    }

    return loader;
}
