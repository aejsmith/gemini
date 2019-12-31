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

#include "Core/Utility.h"

/** Implementation of a fixed size bitmap. */
template <size_t N>
class Bitset
{
public:
    using Element             = uint64_t;

    static constexpr size_t     kNumBits        = N;
    static constexpr size_t     kBitsPerElement = 64;
    static constexpr size_t     kNumElements    = RoundUpPow2(kNumBits, kBitsPerElement) / kBitsPerElement;

public:
                                Bitset();
                                Bitset(const Bitset& inOther);

    Bitset&                     operator =(const Bitset& inOther);

    bool                        Any() const;

    bool                        Test(const size_t inBit) const;

    Bitset&                     Set(const size_t inBit);
    Bitset&                     Clear(const size_t inBit);

    void                        Reset();

    /** Find the first/last set bit, returns N if none are set. */
    size_t                      FindFirst() const;
    size_t                      FindLast() const;

private:
    static size_t               GetElement(const size_t inBit);
    static size_t               GetOffset(const size_t inBit);
    static Element              GetValue(const size_t inBit);

private:
    Element                     mData[kNumElements];

};

template <size_t N>
inline Bitset<N>::Bitset()
{
    Reset();
}

template <size_t N>
inline Bitset<N>::Bitset(const Bitset& inOther)
{
    memcpy(mData, inOther.mData, sizeof(mData));
}

template <size_t N>
inline Bitset<N>& Bitset<N>::operator =(const Bitset& inOther)
{
    mData = inOther.mData;
    return *this;
}

template <size_t N>
inline size_t Bitset<N>::GetElement(const size_t inBit)
{
    Assert(inBit < kNumBits);
    return inBit / kBitsPerElement;
}

template <size_t N>
inline size_t Bitset<N>::GetOffset(const size_t inBit)
{
    return inBit % kBitsPerElement;
}

template <size_t N>
inline typename Bitset<N>::Element Bitset<N>::GetValue(const size_t inBit)
{
    return static_cast<Element>(1) << static_cast<Element>(GetOffset(inBit));
}

template <size_t N>
inline bool Bitset<N>::Any() const
{
    for (size_t i = 0; i < kNumElements; i++)
    {
        if (mData[i])
        {
            return true;
        }
    }

    return false;
}

template <size_t N>
inline bool Bitset<N>::Test(const size_t inBit) const
{
    return (mData[GetElement(inBit)] & GetValue(inBit)) != 0;
}

template <size_t N>
inline Bitset<N>& Bitset<N>::Set(const size_t inBit)
{
    mData[GetElement(inBit)] |= GetValue(inBit);
    return *this;
}

template <size_t N>
inline Bitset<N>& Bitset<N>::Clear(const size_t inBit)
{
    mData[GetElement(inBit)] &= ~GetValue(inBit);
    return *this;
}

template <size_t N>
inline void Bitset<N>::Reset()
{
    memset(mData, 0, sizeof(mData));
}

template <size_t N>
inline size_t Bitset<N>::FindFirst() const
{
    for (size_t i = 0; i < kNumElements; i++)
    {
        if (mData[i])
        {
            return __builtin_ctzll(mData[i]) + (i * kBitsPerElement);
        }
    }

    return kNumBits;
}

template <size_t N>
inline size_t Bitset<N>::FindLast() const
{
    for (long i = kNumElements - 1; i >= 0; i--)
    {
        if (mData[i])
        {
            return (kBitsPerElement - 1 - __builtin_clzll(mData[i])) + (i * kBitsPerElement);
        }
    }

    return kNumBits;
}