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
                            ByteArray(const size_t size);
                            ByteArray(const ByteArray& other);
                            ByteArray(ByteArray&& other);

                            ~ByteArray();

    ByteArray&              operator=(const ByteArray& other);
    ByteArray&              operator=(ByteArray&& other);

                            operator bool() const
                                { return mSize != 0; }

    uint8_t&                operator[](const size_t index)
                                { return mData[index]; }
    const uint8_t&          operator[](const size_t index) const
                                { return mData[index]; }

    size_t                  GetSize() const { return mSize; }

    uint8_t*                Get()           { return mData; }
    const uint8_t*          Get() const     { return mData; }

    /**
     * Resizes the array, optionally reallocating it (and copying content). If
     * the size is being increased, then reallocate must be true. Otherwise,
     * if shrinking and reallocate is false, just the size field will be
     * changed, and the remainder of the allocation will be wasted. This may be
     * useful if only shrinking by a small amount or when the array is only
     * temporary anyway so wastage doesn't matter.
     */
    void                    Resize(const size_t size,
                                   const bool   reallocate = true);

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

inline ByteArray::ByteArray(const size_t size) :
    mSize   (size),
    mData   ((size > 0) ? new uint8_t[size] : nullptr)
{
}

inline ByteArray::ByteArray(const ByteArray& other) :
    ByteArray   (other.mSize)
{
    memcpy(mData, other.mData, mSize);
}

inline ByteArray::ByteArray(ByteArray&& other) :
    mSize   (other.mSize),
    mData   (other.mData)
{
    other.mSize = 0;
    other.mData = nullptr;
}

inline ByteArray::~ByteArray()
{
    delete[] mData;
}

inline ByteArray& ByteArray::operator=(const ByteArray& other)
{
    if (this != &other)
    {
        if (mSize != other.mSize)
        {
            delete[] mData;

            mData = new uint8_t[other.mSize];
            mSize = other.mSize;
        }

        memcpy(mData, other.mData, mSize);
    }

    return *this;
}

inline ByteArray& ByteArray::operator=(ByteArray&& other)
{
    if (this != &other)
    {
        delete[] mData;

        mSize = other.mSize;
        mData = other.mData;

        other.mSize = 0;
        other.mData = nullptr;
    }

    return *this;
}

inline void ByteArray::Resize(const size_t size,
                              const bool   reallocate)
{
    if (size != mSize)
    {
        Assert(reallocate || size < mSize);

        if (reallocate)
        {
            uint8_t* const newData = (size != 0) ? new uint8_t[size] : 0;

            if (mSize != 0 && size != 0)
            {
                memcpy(newData, mData, std::min(size, mSize));
            }

            delete[] mData;
            mData = newData;
        }

        mSize = size;
    }
}

inline void ByteArray::Clear()
{
    delete[] mData;

    mSize = 0;
    mData = nullptr;
}
