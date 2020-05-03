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

#include <algorithm>
#include <atomic>
#include <type_traits>

/**
 * This class provides reference counting functionality to derived classes. It
 * maintains a reference count which is modified using the Retain() and
 * Release() methods. When the reference count reaches 0, the Released() method
 * is called, this method can be overridden for custom functionality. The
 * default implementation deletes the object.
 *
 * The Retain and Release methods are marked const to allow for const pointers
 * to a reference counted object.
 */
class RefCounted
{
public:
                                    RefCounted();

public:
    int32_t                         Retain() const;
    int32_t                         Release() const;

    int32_t                         GetRefCount() const { return mRefCount; }

protected:
    virtual                         ~RefCounted();

    virtual void                    Released();

private:
    mutable std::atomic<int32_t>    mRefCount;

};

inline RefCounted::RefCounted() :
    mRefCount (0)
{
}

inline int32_t RefCounted::Retain() const
{
    return mRefCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

inline int32_t RefCounted::Release() const
{
    Assert(mRefCount > 0);

    const int32_t result = mRefCount.fetch_sub(1, std::memory_order_release) - 1;

    if (result == 0)
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        const_cast<RefCounted*>(this)->Released();
    }

    return result;
}

/**
 * This class implements a smart pointer to a reference counted object which
 * provides Retain() and Release() methods (need not necessarily derive from
 * RefCounted).
 *
 * This class allows implicit conversions to and from pointers to the referenced
 * type. It is typically safe to take raw pointers to reference counted objects
 * as arguments as long as you expect that the caller should hold a reference.
 * Similarly, it should be safe to return raw pointers to objects as long as
 * you know a reference should be held to it somewhere. If the caller intends
 * to store the returned pointer for long term usage, it should assign it to a
 * RefPtr.
 */
template <typename T>
class RefPtr
{
public:
    using ReferencedType          = T;

    template <typename U>
    using EnableIfConvertible     = typename std::enable_if<std::is_convertible<U*, ReferencedType*>::value, RefPtr&>::type;

public:
                                    RefPtr();
                                    RefPtr(std::nullptr_t);
                                    RefPtr(RefPtr&& other);
                                    RefPtr(const RefPtr& other);

                                    template <typename U, typename = EnableIfConvertible<U>>
                                    RefPtr(U* const object);

                                    template <typename U, typename = EnableIfConvertible<U>>
                                    RefPtr(const RefPtr<U>& other);

                                    template <typename U, typename = EnableIfConvertible<U>>
                                    RefPtr(RefPtr<U>&& other);

                                    ~RefPtr();

public:
    RefPtr&                         operator =(RefPtr&& other);
    RefPtr&                         operator =(const RefPtr& other);
    RefPtr&                         operator =(ReferencedType* const object);

    template <typename U>
    EnableIfConvertible<U>          operator =(const RefPtr<U>& other);

    template <typename U>
    EnableIfConvertible<U>          operator =(RefPtr<U>&& other);

    explicit                        operator bool() const               { return mObject != nullptr; }

                                    operator ReferencedType*() const    { return mObject; }

    ReferencedType&                 operator *() const                  { return *mObject; }
    ReferencedType*                 operator ->() const                 { return mObject; }

    ReferencedType*                 Get() const                         { return mObject; }

    /**
     * Change the object that the pointer refers to. retain indicates whether
     * to retain the object, defaults to true. If false, the caller should have
     * already added a reference to the object, which will be taken over by
     * this RefPtr.
     */
    void                            Reset(ReferencedType* const object = nullptr,
                                          const bool            retain = true);

    /**
     * Detach the referenced object, if any. A raw pointer to it will be
     * returned, without releasing the reference, and the RefPtr will be
     * set to null. It is the caller's responsibility to ensure that the
     * reference is released later.
     */
    ReferencedType*                 Detach();

    void                            Swap(RefPtr& other);

    template <typename U>
    RefPtr<U>                       StaticCast() const;

private:
    ReferencedType*                 mObject;

};

template <typename T>
inline RefPtr<T>::RefPtr() :
    mObject (nullptr)
{
}

template <typename T>
inline RefPtr<T>::RefPtr(std::nullptr_t) :
    mObject (nullptr)
{
}

template <typename T> template <typename U, typename>
inline RefPtr<T>::RefPtr(U* const object) :
    mObject (object)
{
    if (mObject)
    {
        mObject->Retain();
    }
}

template <typename T>
inline RefPtr<T>::RefPtr(const RefPtr& other) :
    mObject (other.mObject)
{
    if (mObject)
    {
        mObject->Retain();
    }
}

template <typename T> template <typename U, typename>
inline RefPtr<T>::RefPtr(const RefPtr<U>& other) :
    mObject (other.Get())
{
    if (mObject)
    {
        mObject->Retain();
    }
}

template <typename T> template <typename U, typename>
inline RefPtr<T>::RefPtr(RefPtr<U>&& other) :
    mObject (other.Detach())
{
}

template <typename T>
inline RefPtr<T>::RefPtr(RefPtr&& other) :
    mObject (other.Detach())
{
}

template <typename T>
inline RefPtr<T>::~RefPtr()
{
    Reset();
}

template <typename T>
inline RefPtr<T>& RefPtr<T>::operator =(ReferencedType* const object)
{
    Reset(object);
    return *this;
}

template <typename T>
inline RefPtr<T>& RefPtr<T>::operator =(const RefPtr& other)
{
    Reset(other.mObject);
    return *this;
}

template <typename T> template <typename U>
inline typename RefPtr<T>::template EnableIfConvertible<U>
RefPtr<T>::operator =(const RefPtr<U>& other)
{
    Reset(other.Get());
    return *this;
}

template <typename T> template <typename U>
inline typename RefPtr<T>::template EnableIfConvertible<U>
RefPtr<T>::operator =(RefPtr<U>&& other)
{
    if (mObject)
    {
        mObject->Release();
    }

    mObject = other.Detach();

    return *this;
}

template <typename T>
RefPtr<T>& RefPtr<T>::operator =(RefPtr&& other)
{
    if (mObject)
    {
        mObject->Release();
    }

    mObject = other.Detach();

    return *this;
}

template <typename T>
void RefPtr<T>::Reset(ReferencedType* const object,
                            const bool            retain)
{
    if (object && retain)
    {
        object->Retain();
    }

    if (mObject)
    {
        mObject->Release();
    }

    mObject = object;
}

template <typename T>
T* RefPtr<T>::Detach()
{
    T* const result = mObject;
    mObject = nullptr;
    return result;
}

template <typename T>
void RefPtr<T>::Swap(RefPtr& other)
{
    std::swap(mObject, other.mObject);
}

template <typename T> template <typename U>
RefPtr<U> RefPtr<T>::StaticCast() const
{
    return RefPtr<U>(static_cast<U*>(mObject));
}

template <typename T, typename U>
inline bool operator ==(const RefPtr<T>& a, const RefPtr<U>& b)
{
    return a.Get() == b.Get();
}

template <typename T, typename U>
inline bool operator !=(const RefPtr<T>& a, const RefPtr<U>& b)
{
    return !(a == b);
}

template <typename T, typename U>
inline bool operator <(const RefPtr<T>& a, const RefPtr<U>& b)
{
    return a.Get() < b.Get();
}
