/*
 * Copyright (C) 2018 Alex Smith
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
    }

/**
 * Helper functions.
 */

/** Allocate an array of a given type on the stack. */
#define AllocateStackArray(Type, inCount) \
    reinterpret_cast<Type*>(alloca(sizeof(Type) * inCount))

/** Allocate an array of a given type on the stack, cleared to zero. */
#define AllocateZeroedStackArray(Type, inCount) \
    reinterpret_cast<Type*>(memset(alloca(sizeof(Type) * inCount), 0, sizeof(Type) * inCount))

/** Get the size of an array. */
template <typename T, size_t N>
constexpr size_t ArraySize(T (&array)[N])
{
    return N;
}

/** Test that only one bit is set in a value. */
template <typename T>
constexpr bool IsOnlyOneBitSet(const T& inValue)
{
    return inValue && !(inValue & (inValue - 1));
}

/** Test that a value is a power of 2. */
template <typename T>
constexpr bool IsPowerOf2(const T& inValue)
{
    return IsOnlyOneBitSet(inValue);
}

/**
 * Scope guard helper.
 */

template <typename Function>
class ScopeGuard
{
public:
                                ScopeGuard(Function inFunction);
                                ScopeGuard(ScopeGuard&& inOther);
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
ScopeGuard<Function>::ScopeGuard(Function inFunction) :
    mFunction   (std::move(inFunction)),
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
ScopeGuard<Function>::ScopeGuard(ScopeGuard&& inOther) :
    mFunction   (std::move(inOther.mFunction)),
    mActive     (inOther.mActive)
{
    inOther.Cancel();
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
ScopeGuard<Function> MakeScopeGuard(Function inFunction)
{
    return ScopeGuard<Function>(std::move(inFunction));
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
