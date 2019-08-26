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

#include <xxHash.h>

#include <cstring>
#include <string>
#include <type_traits>

inline size_t HashData(const void*    inData,
                       const size_t   inSize,
                       const uint64_t inSeed = 0)
{
    return Gemini_XXH64(inData, inSize, inSeed);
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value, size_t>::type
HashValue(const T inValue)
{
    /* This is probably reasonable for many cases. */
    return static_cast<size_t>(inValue);
}

template <typename T>
inline typename std::enable_if<std::is_enum<T>::value, size_t>::type
HashValue(const T inValue)
{
    return HashValue(static_cast<typename std::underlying_type<T>::type>(inValue));
}

template <typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, size_t>::type
HashValue(const T inValue)
{
    /* Generate the same hash for -0.0 and 0.0. */
    if (inValue == static_cast<T>(0))
    {
        return 0;
    }

    return HashData(&inValue, sizeof(inValue));
}

/** Hash a pointer (the pointer itself, not what it refers to). */
template <typename T>
inline size_t HashValue(T* const inValue)
{
    return reinterpret_cast<size_t>(inValue);
}

inline size_t HashValue(const std::string& inValue)
{
    return HashData(inValue.c_str(), inValue.length());
}

/**
 * This function can be called repeatedly to generate a hash value from several
 * variables, like so:
 *
 *  size_t hash = HashValue(valA);
 *  hash = HashCombine(hash, valB);
 *  hash = HashCombine(hash, valC);
 *  ...
 *
 * It uses HashValue() to hash the value. Note that for values that are
 * contiguous in memory it is likely faster to hash the entire range using
 * HashData().
 */
template <typename T>
inline size_t HashCombine(const size_t inSeed, const T& inValue)
{
    /* Borrowed from boost::hash_combine(). */
    size_t hash   = HashValue(inValue);
    size_t result = inSeed ^ (hash + 0x9e3779b9 + (inSeed << 6) + (inSeed >> 2));

    return result;
}

/**
 * Provides a hash functor for use with hash-based containers which require a
 * function object for calculating a hash. Works with any type for which
 * HashValue() is defined.
 */
template <typename T>
struct Hash
{
    using argument_type       = T;
    using result_type         = size_t;

    result_type                 operator()(const T& inValue) const { return HashValue(inValue); }
};

/**
 * Define HashValue() and operator== for a type which will directly hash/compare
 * the whole memory occupied by an object. Care must be taken to ensure that any
 * padding within the object is zeroed (e.g. memset the whole object upon
 * construction), so that hashing/comparison will always yield the same results
 * for identical objects (without doing this, padding could have different
 * content in different objects resulting in different results).
 */
#define DEFINE_HASH_MEM_OPS(T) \
    inline size_t HashValue(const T& inValue) \
    { \
        return HashData(&inValue, sizeof(T)); \
    } \
    \
    inline bool operator==(const T& inA, const T& inB) \
    { \
        return memcmp(&inA, &inB, sizeof(T)) == 0; \
    }
