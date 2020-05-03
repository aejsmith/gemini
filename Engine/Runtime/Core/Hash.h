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

#include "Core/CoreDefs.h"

#include <xxHash.h>

#include <cstring>
#include <string>
#include <type_traits>

inline size_t HashData(const void*    data,
                       const size_t   size,
                       const uint64_t seed = 0)
{
    return Gemini_XXH64(data, size, seed);
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value, size_t>::type
HashValue(const T value)
{
    /* This is probably reasonable for many cases. */
    return static_cast<size_t>(value);
}

template <typename T>
inline typename std::enable_if<std::is_enum<T>::value, size_t>::type
HashValue(const T value)
{
    return HashValue(static_cast<typename std::underlying_type<T>::type>(value));
}

template <typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, size_t>::type
HashValue(const T value)
{
    /* Generate the same hash for -0.0 and 0.0. */
    if (value == static_cast<T>(0))
    {
        return 0;
    }

    return HashData(&value, sizeof(value));
}

/** Hash a pointer (the pointer itself, not what it refers to). */
template <typename T>
inline size_t HashValue(T* const value)
{
    return reinterpret_cast<size_t>(value);
}

inline size_t HashValue(const std::string& value)
{
    return HashData(value.c_str(), value.length());
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
inline size_t HashCombine(const size_t seed, const T& value)
{
    /* Borrowed from boost::hash_combine(). */
    size_t hash   = HashValue(value);
    size_t result = seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));

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

    result_type                 operator()(const T& value) const { return HashValue(value); }
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
    inline size_t HashValue(const T& value) \
    { \
        return HashData(&value, sizeof(T)); \
    } \
    \
    inline bool operator==(const T& a, const T& b) \
    { \
        return memcmp(&a, &b, sizeof(T)) == 0; \
    }
