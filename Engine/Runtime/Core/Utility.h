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

#include <cstring>
#include <type_traits>

/**
 * Helper macros.
 */

/** Define bitwise operators for an enum. */
#define DEFINE_ENUM_BITWISE_OPS(Type) \
    inline constexpr Type operator |(const Type a, const Type b) \
    { \
        using UnderlyingType = std::underlying_type<Type>::type; \
        return static_cast<Type>(static_cast<UnderlyingType>(a) | static_cast<UnderlyingType>(b)); \
    } \
    inline constexpr Type operator &(const Type a, const Type b) \
    { \
        using UnderlyingType = std::underlying_type<Type>::type; \
        return static_cast<Type>(static_cast<UnderlyingType>(a) & static_cast<UnderlyingType>(b)); \
    } \
    inline constexpr Type& operator |=(Type& a, const Type b) \
    { \
        return a = a | b; \
    } \
    inline constexpr Type& operator &=(Type& a, const Type b) \
    { \
        return a = a & b; \
    }

/**
 * Helper functions.
 */

/** Allocate an array of a given type on the stack. */
#define AllocateStackArray(Type, count) \
    reinterpret_cast<Type*>(alloca(sizeof(Type) * count))

/** Allocate an array of a given type on the stack, cleared to zero. */
#define AllocateZeroedStackArray(Type, count) \
    reinterpret_cast<Type*>(memset(alloca(sizeof(Type) * count), 0, sizeof(Type) * count))

/** Get the size of an array. */
template <typename T, size_t N>
constexpr size_t ArraySize(T (&array)[N])
{
    return N;
}

/** Test that only one bit is set in a value. */
template <typename T>
constexpr bool IsOnlyOneBitSet(const T& value)
{
    return value && !(value & (value - 1));
}

/** Test that a value is a power of 2. */
template <typename T>
constexpr bool IsPowerOf2(const T& value)
{
    return IsOnlyOneBitSet(value);
}

/** Round an integer value up to a power of 2. */
template <typename T>
constexpr T RoundUpPow2(const T& value, const T& nearest)
{
    return (value & (nearest - 1))
               ? ((value + nearest) & ~(nearest - 1))
               : value;
}

/** Round a value down to a power of 2. */
template <typename T>
constexpr T RoundDownPow2(const T& value, const T& nearest)
{
    return (value & (nearest - 1))
               ? (value & ~(nearest - 1))
               : value;
}

/** Round an integer value up. Use RoundUpPow2() if known as a power of 2. */
template <typename T>
constexpr T RoundUp(const T& value, const T& nearest)
{
    return (value % nearest)
               ? (value - (value % nearest)) + nearest
               : value;
}

/** Round an integer value down. Use RoundDownPow2() if known as a power of 2. */
template <typename T>
constexpr T RoundDown(const T& value, const T& nearest)
{
    return (value % nearest)
               ? value - (value % nearest)
               : value;
}

/**
 * Scope guard helper.
 */

template <typename Function>
class ScopeGuard
{
public:
                                ScopeGuard(Function function);
                                ScopeGuard(ScopeGuard&& other);
                                ~ScopeGuard();

    void                        Cancel();

private:
                                ScopeGuard() = delete;
                                ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard&                 operator =(const ScopeGuard&) = delete;

private:
    Function                    mFunction;
    bool                        mActive;

};

template <typename Function>
ScopeGuard<Function>::ScopeGuard(Function function) :
    mFunction   (std::move(function)),
    mActive     (true)
{
}

template <typename Function>
ScopeGuard<Function>::~ScopeGuard()
{
    if (mActive)
    {
        mFunction();
    }
}

template <typename Function>
ScopeGuard<Function>::ScopeGuard(ScopeGuard&& other) :
    mFunction   (std::move(other.mFunction)),
    mActive     (other.mActive)
{
    other.Cancel();
}

template <typename Function>
void ScopeGuard<Function>::Cancel()
{
    mActive = false;
}

/**
 * Helper to call a function at the end of a scope: the returned object will
 * call the specified function when it is destroyed, unless cancelled by
 * calling Cancel() on it. Example:
 *
 *   auto guard = MakeScopeGuard([] { Action(); });
 *
 * Will call Action() when guard goes out of scope.
 */
template <typename Function>
ScopeGuard<Function> MakeScopeGuard(Function function)
{
    return ScopeGuard<Function>(std::move(function));
}

/**
 * Helper to restrict a method to only be called by a given class. For example:
 *
 *     void Foo(OnlyCalledBy<Bar>);
 *
 * restricts Foo to only be called by the class Bar. The class can only be
 * constructed by Bar, so no other classes can construct the parameter
 * necessary to call the method.
 */
template <typename T>
class OnlyCalledBy
{
private:
                                OnlyCalledBy() {}

    friend T;
};

/** Base class ensuring derived classes are not copyable. */
struct Uncopyable
{
                                Uncopyable()                    = default;
                                Uncopyable(const Uncopyable&)   = delete;

    Uncopyable&                 operator =(const Uncopyable&)   = delete;

};
