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
 * ReferencePtr.
 */
template <typename T>
class ReferencePtr
{
public:
    using ReferencedType          = T;

    template <typename U>
    using EnableIfConvertible     = typename std::enable_if<std::is_convertible<U*, ReferencedType*>::value, ReferencePtr&>::type;

public:
                                    ReferencePtr();
                                    ReferencePtr(std::nullptr_t);
                                    ReferencePtr(ReferencePtr&& inOther);
                                    ReferencePtr(const ReferencePtr& inOther);

                                    template <typename U, typename = EnableIfConvertible<U>>
                                    ReferencePtr(U* const inObject);

                                    template <typename U, typename = EnableIfConvertible<U>>
                                    ReferencePtr(const ReferencePtr<U>& inOther);

                                    template <typename U, typename = EnableIfConvertible<U>>
                                    ReferencePtr(ReferencePtr<U>&& inOther);

                                    ~ReferencePtr();

public:
    ReferencePtr&                   operator =(ReferencePtr&& inOther);
    ReferencePtr&                   operator =(const ReferencePtr& inOther);
    ReferencePtr&                   operator =(ReferencedType* const inObject);

    template <typename U>
    EnableIfConvertible<U>          operator =(const ReferencePtr<U>& inOther);

    template <typename U>
    EnableIfConvertible<U>          operator =(ReferencePtr<U>&& inOther);

    explicit                        operator bool() const               { return mObject != nullptr; }

                                    operator ReferencedType*() const    { return mObject; }

    ReferencedType&                 operator *() const                  { return *mObject; }
    ReferencedType*                 operator ->() const                 { return mObject; }

    ReferencedType*                 Get() const                         { return mObject; }

    /**
     * Change the object that the pointer refers to. inRetain indicates whether
     * to retain the object, defaults to true. If false, the caller should have
     * already added a reference to the object, which will be taken over by
     * this ReferencePtr.
     */
    void                            Reset(ReferencedType* const inObject = nullptr,
                                          const bool            inRetain = true);

    /**
     * Detach the referenced object, if any. A raw pointer to it will be
     * returned, without releasing the reference, and the ReferencePtr will be
     * set to null. It is the caller's responsibility to ensure that the
     * reference is released later.
     */
    ReferencedType*                 Detach();

    void                            Swap(ReferencePtr& inOther);

    template <typename U>
    ReferencePtr<U>                 StaticCast() const;

private:
    ReferencedType*                 mObject;

};

template <typename T>
inline ReferencePtr<T>::ReferencePtr() :
    mObject (nullptr)
{
}

template <typename T>
inline ReferencePtr<T>::ReferencePtr(std::nullptr_t) :
    mObject (nullptr)
{
}

template <typename T> template <typename U, typename>
inline ReferencePtr<T>::ReferencePtr(U* const inObject) :
    mObject (inObject)
{
    if (mObject)
    {
        mObject->Retain();
    }
}

template <typename T>
inline ReferencePtr<T>::ReferencePtr(const ReferencePtr& inOther) :
    mObject (inOther.mObject)
{
    if (mObject)
    {
        mObject->Retain();
    }
}

template <typename T> template <typename U, typename>
inline ReferencePtr<T>::ReferencePtr(const ReferencePtr<U>& inOther) :
    mObject (inOther.Get())
{
    if (mObject)
    {
        mObject->Retain();
    }
}

template <typename T> template <typename U, typename>
inline ReferencePtr<T>::ReferencePtr(ReferencePtr<U>&& inOther) :
    mObject (inOther.Detach())
{
}

template <typename T>
inline ReferencePtr<T>::ReferencePtr(ReferencePtr&& inOther) :
    mObject (inOther.Detach())
{
}

template <typename T>
inline ReferencePtr<T>::~ReferencePtr()
{
    Reset();
}

template <typename T>
inline ReferencePtr<T>& ReferencePtr<T>::operator =(ReferencedType* const inObject)
{
    Reset(inObject);
    return *this;
}

template <typename T>
inline ReferencePtr<T>& ReferencePtr<T>::operator =(const ReferencePtr& inOther)
{
    Reset(inOther.mObject);
    return *this;
}

template <typename T> template <typename U>
inline typename ReferencePtr<T>::template EnableIfConvertible<U>
ReferencePtr<T>::operator =(const ReferencePtr<U>& inOther)
{
    Reset(inOther.Get());
    return *this;
}

template <typename T> template <typename U>
inline typename ReferencePtr<T>::template EnableIfConvertible<U>
ReferencePtr<T>::operator =(ReferencePtr<U>&& inOther)
{
    if (mObject)
    {
        mObject->Release();
    }

    mObject = inOther.Detach();

    return *this;
}

template <typename T>
ReferencePtr<T>& ReferencePtr<T>::operator =(ReferencePtr&& inOther)
{
    if (mObject)
    {
        mObject->Release();
    }

    mObject = inOther.Detach();

    return *this;
}

template <typename T>
void ReferencePtr<T>::Reset(ReferencedType* const inObject,
                            const bool            inRetain)
{
    if (inObject && inRetain)
    {
        inObject->Retain();
    }

    if (mObject)
    {
        mObject->Release();
    }

    mObject = inObject;
}

template <typename T>
T* ReferencePtr<T>::Detach()
{
    T* const result = mObject;
    mObject = nullptr;
    return result;
}

template <typename T>
void ReferencePtr<T>::Swap(ReferencePtr& inOther)
{
    std::swap(mObject, inOther.mObject);
}

template <typename T> template <typename U>
ReferencePtr<U> ReferencePtr<T>::StaticCast() const
{
    return ReferencePtr<U>(static_cast<U*>(mObject));
}

template <typename T, typename U>
inline bool operator ==(const ReferencePtr<T>& a, const ReferencePtr<U>& b)
{
    return a.Get() == b.Get();
}

template <typename T, typename U>
inline bool operator !=(const ReferencePtr<T>& a, const ReferencePtr<U>& b)
{
    return !(a == b);
}

template <typename T, typename U>
inline bool operator <(const ReferencePtr<T>& a, const ReferencePtr<U>& b)
{
    return a.Get() < b.Get();
}
