/*
 * Copyright (C) 2018-2020 Alex Smith
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

#ifdef _MSC_VER
#include <intrin.h>
#endif

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
                                Bitset(const Bitset& other);

    Bitset&                     operator =(const Bitset& other);

    bool                        Any() const;

    bool                        Test(const size_t bit) const;

    Bitset&                     Set(const size_t bit);
    Bitset&                     Clear(const size_t bit);

    void                        Reset();

    /** Find the first/last set bit, returns N if none are set. */
    size_t                      FindFirst() const;
    size_t                      FindLast() const;

private:
    static size_t               GetElement(const size_t bit);
    static size_t               GetOffset(const size_t bit);
    static Element              GetValue(const size_t bit);

    static size_t               CTZ(const Element val);
    static size_t               CLZ(const Element val);

private:
    Element                     mData[kNumElements];

};

template <size_t N>
inline Bitset<N>::Bitset()
{
    Reset();
}

template <size_t N>
inline Bitset<N>::Bitset(const Bitset& other)
{
    memcpy(mData, other.mData, sizeof(mData));
}

template <size_t N>
inline Bitset<N>& Bitset<N>::operator =(const Bitset& other)
{
    mData = other.mData;
    return *this;
}

template <size_t N>
inline size_t Bitset<N>::GetElement(const size_t bit)
{
    Assert(bit < kNumBits);
    return bit / kBitsPerElement;
}

template <size_t N>
inline size_t Bitset<N>::GetOffset(const size_t bit)
{
    return bit % kBitsPerElement;
}

template <size_t N>
inline typename Bitset<N>::Element Bitset<N>::GetValue(const size_t bit)
{
    return static_cast<Element>(1) << static_cast<Element>(GetOffset(bit));
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
inline bool Bitset<N>::Test(const size_t bit) const
{
    return (mData[GetElement(bit)] & GetValue(bit)) != 0;
}

template <size_t N>
inline Bitset<N>& Bitset<N>::Set(const size_t bit)
{
    mData[GetElement(bit)] |= GetValue(bit);
    return *this;
}

template <size_t N>
inline Bitset<N>& Bitset<N>::Clear(const size_t bit)
{
    mData[GetElement(bit)] &= ~GetValue(bit);
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
            return CTZ(mData[i]) + (i * kBitsPerElement);
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
            return (kBitsPerElement - 1 - CLZ(mData[i])) + (i * kBitsPerElement);
        }
    }

    return kNumBits;
}

template <size_t N>
inline size_t Bitset<N>::CTZ(const Element val)
{
    #ifdef _MSC_VER
        unsigned long ret;
        _BitScanForward64(&ret, val);
        return static_cast<size_t>(ret);
    #else
        return __builtin_ctzll(val);
    #endif
}

template <size_t N>
inline size_t Bitset<N>::CLZ(const Element val)
{
    #ifdef _MSC_VER
        return static_cast<size_t>(__lzcnt64(val));
    #else
        return __builtin_clzll(val);
    #endif
}
