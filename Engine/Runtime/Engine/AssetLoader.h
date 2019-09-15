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

#include "Core/DataStream.h"

#include "Engine/Asset.h"

class AssetLoader : public Object
{
    CLASS();

public:
    /** Get the file extension which this loader handles. */
    virtual const char*             GetExtension() const = 0;

    AssetPtr                        Load(DataStream* const inData,
                                         const char* const inPath);

    DataStream*                     GetData()       { return mData; }
    const DataStream*               GetData() const { return mData; }

    static ObjectPtr<AssetLoader>   Create(const std::string& inExtension);

protected:
                                    AssetLoader();
                                    ~AssetLoader();

    virtual AssetPtr                Load() = 0;

protected:
    DataStream*                     mData;
    const char*                     mPath;

};
