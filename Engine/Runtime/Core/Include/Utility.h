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

#include "CoreDefs.h"

/**
 * Helper functions.
 */

/** Get the size of an array. */
template <typename T, size_t N>
constexpr size_t ArraySize(T (&array)[N])
{
    return N;
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
