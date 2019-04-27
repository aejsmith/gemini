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

#include "Core/CoreDefs.h"

#include <cstring>

/**
 * Class encapsulating a fixed size, dynamically allocated byte array.
 */
class ByteArray
{
public:
                            ByteArray();
                            ByteArray(const size_t inSize);
                            ByteArray(const ByteArray& inOther);
                            ByteArray(ByteArray&& inOther);

                            ~ByteArray();

    ByteArray&              operator=(const ByteArray& inOther);
    ByteArray&              operator=(ByteArray&& inOther);

                            operator bool() const
                                { return mSize != 0; }

    size_t                  GetSize() const { return mSize; }

    uint8_t*                Get()           { return mData; }
    const uint8_t*          Get() const     { return mData; }

    void                    Clear();

private:
    size_t                  mSize;
    uint8_t*                mData;
};

inline ByteArray::ByteArray() :
    mSize   (0),
    mData   (0)
{
}

inline ByteArray::ByteArray(const size_t inSize) :
    mSize   (inSize),
    mData   (new uint8_t[inSize])
{
}

inline ByteArray::ByteArray(const ByteArray& inOther) :
    ByteArray   (inOther.mSize)
{
    memcpy(mData, inOther.mData, mSize);
}

inline ByteArray::ByteArray(ByteArray&& inOther) :
    mSize   (inOther.mSize),
    mData   (inOther.mData)
{
    inOther.mSize = 0;
    inOther.mData = nullptr;
}

inline ByteArray::~ByteArray()
{
    delete[] mData;
}

inline ByteArray& ByteArray::operator=(const ByteArray& inOther)
{
    if (this != &inOther)
    {
        if (mSize != inOther.mSize)
        {
            delete[] mData;

            mData = new uint8_t[inOther.mSize];
            mSize = inOther.mSize;
        }

        memcpy(mData, inOther.mData, mSize);
    }

    return *this;
}

inline ByteArray& ByteArray::operator=(ByteArray&& inOther)
{
    if (this != &inOther)
    {
        delete[] mData;

        mSize = inOther.mSize;
        mData = inOther.mData;

        inOther.mSize = 0;
        inOther.mData = nullptr;
    }

    return *this;
}

inline void ByteArray::Clear()
{
    delete[] mData;

    mSize = 0;
    mData = nullptr;
}