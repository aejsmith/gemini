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

#include "Core/Utility.h"

/**
 * Template base for a singleton class. Users must add the implementation in
 * a .cpp file with SINGLETON_IMPL.
 */
template <typename T>
class Singleton : Uncopyable
{
protected:
                                    Singleton();
                                    ~Singleton();

public:
    /** Get the singleton instance of the class. */
    static T&                       Get();

    /** Check whether an instance of this class exists. */
    static bool                     HasInstance();

private:
    static T*                       sInstance;

};

#define SINGLETON_IMPL(T) \
    template <> T* Singleton<T>::sInstance = nullptr

template <typename T>
Singleton<T>::Singleton()
{
    Assert(!sInstance);

    sInstance = static_cast<T*>(this);
}

template <typename T>
Singleton<T>::~Singleton()
{
    Assert(sInstance == this);

    sInstance = static_cast<T*>(this);
}

template <typename T>
T& Singleton<T>::Get()
{
    Assert(HasInstance());

    return *sInstance;
}

template <typename T>
bool Singleton<T>::HasInstance()
{
    return sInstance != nullptr;
}
