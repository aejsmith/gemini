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

AssetPtr AssetLoader::Load(DataStream* const data,
                           const char* const path)
{
    mData = data;
    mPath = path;

    return Load();
}

ObjPtr<AssetLoader> AssetLoader::Create(const std::string& extension)
{
    /* Map of file types to loader class. */
    static auto sTypeMap =
        [] ()
        {
            std::map<std::string, const MetaClass*> map;

            MetaClass::Visit(
                [&] (const MetaClass& metaClass)
                {
                    if (AssetLoader::staticMetaClass.IsBaseOf(metaClass) && metaClass.IsConstructable())
                    {
                        ObjPtr<Object> object      = metaClass.Construct();
                        ObjPtr<AssetLoader> loader = object.StaticCast<AssetLoader>();

                        const char* const extension = loader->GetExtension();
                        
                        map.insert(std::make_pair(extension, &metaClass));
                    }
                });

            return map;
        }();

    ObjPtr<AssetLoader> loader;

    auto ret = sTypeMap.find(extension);
    if (ret != sTypeMap.end())
    {
        ObjPtr<Object> object = ret->second->Construct();
        loader = object.StaticCast<AssetLoader>();
    }

    return loader;
}
