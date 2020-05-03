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

#include "Core/Intrusive.h"

/**
 * Node for an intrusive list. Usage is to embed this inside the type being
 * stored in a list and point to it with IntrusiveList's node template
 * parameter.
 *
 * Note that copying a node (e.g. by copying the object containing it) does not
 * copy or change membership: the copy constructor initialises the node as not
 * attached to any list, while the copy assignment operator does not do anything
 * (leaves the destination attached to any list it is currently attached to).
 */
class IntrusiveListNode
{
public:
                                IntrusiveListNode();
                                IntrusiveListNode(const IntrusiveListNode& other);
                                ~IntrusiveListNode();

    IntrusiveListNode&          operator=(const IntrusiveListNode& other);

    bool                        IsInserted() const;

private:
    IntrusiveListNode*          mPrevious;
    IntrusiveListNode*          mNext;

    template <typename T, IntrusiveListNode T::*Member>
    friend class IntrusiveList;
};

inline IntrusiveListNode::IntrusiveListNode() :
    mPrevious   (this),
    mNext       (this)
{
}

inline IntrusiveListNode::IntrusiveListNode(const IntrusiveListNode& other) :
    mPrevious   (this),
    mNext       (this)
{
}

inline IntrusiveListNode::~IntrusiveListNode()
{
    Assert(!IsInserted());
}

inline bool IntrusiveListNode::IsInserted() const
{
    return mNext != this;
}

/**
 * Intrusive doubly-linked list class. Stores the node as part of the objects
 * contained in the list, which means no additional allocation needs to be
 * performed by the container, and also allows constant-time removal of objects
 * from the list given a pointer to the object.
 *
 * Note that while the list provides a copy constructor/assignment operator,
 * they do not copy the list content, as objects only have one node and can
 * therefore only be in one list at a time. The move constructor/assignment
 * operator do, however, move the list content and leave the original list
 * empty.
 */
template <typename T, IntrusiveListNode T::*Member>
class IntrusiveList
{
private:
    using Impl                = IntrusiveImpl<T, IntrusiveListNode, Member>;

public:
    template <typename U, typename N>
    class BaseIterator
    {
    public:
                                BaseIterator() : mNode (nullptr) {}
                                BaseIterator(const BaseIterator& other) : mNode (other.mNode) {}
                                BaseIterator(N* const node) : mNode (node) {}

        BaseIterator&           operator=(const BaseIterator& other)
                                    { mNode = other.mNode; return *this; }

        bool                    operator==(const BaseIterator& other) const
                                    { return mNode == other.mNode; }
        bool                    operator!=(const BaseIterator& other) const
                                    { return mNode != other.mNode; }

        U*                      operator*() const   { return Impl::GetValue(mNode); }
        U*                      operator->() const  { return Impl::GetValue(mNode); }

        BaseIterator&           operator++()
                                    { mNode = mNode->mNext; return *this; }
        BaseIterator            operator++(int)
                                    { BaseIterator ret = *this; mNode = mNode->mNext; return ret; }
        BaseIterator&           operator--()
                                    { mNode = mNode->mPrevious; return *this; }
        BaseIterator            operator--(int)
                                    { BaseIterator ret = *this; mNode = mNode->mPrevious; return ret; }

    private:
        N*                      mNode;

    };

    using Iterator            = BaseIterator<T, IntrusiveListNode>;
    using ConstIterator       = BaseIterator<const T, const IntrusiveListNode>;

public:
                                IntrusiveList();
                                IntrusiveList(const IntrusiveList& other);
                                IntrusiveList(IntrusiveList&& other);
                                ~IntrusiveList();

    IntrusiveList&              operator=(const IntrusiveList& other);
    IntrusiveList&              operator=(IntrusiveList&& other);

    bool                        IsEmpty() const;

    void                        Append(T* const value);
    void                        Prepend(T* const value);
    void                        InsertBefore(T* const position,
                                             T* const value);
    void                        InsertAfter(T* const position,
                                            T* const value);

    void                        Remove(T* const value);
    T*                          RemoveFirst();
    T*                          RemoveLast();
    void                        Clear();

    T*                          First() const;
    T*                          Last() const;

    /**
     * Return a pointer to the preceding/following element in the list, or null
     * if the element is the first/last element, respectively.
     */
    T*                          Previous(T* const current) const;
    const T*                    Previous(const T* const current) const;
    T*                          Next(T* const current) const;
    const T*                    Next(const T* const current) const;

    /**
     * Iterators for use with range-based for loops. The other functions should
     * be preferred over iterators outside of range-based loops.
     */
    Iterator                    begin()         { return Iterator(mHead.mNext); }
    ConstIterator               begin() const   { return ConstIterator(mHead.mNext); }
    Iterator                    end()           { return Iterator(&mHead); }
    ConstIterator               end() const     { return ConstIterator(&mHead); }

private:
    static void                 InsertBefore(IntrusiveListNode* const position,
                                             IntrusiveListNode* const node);
    static void                 InsertAfter(IntrusiveListNode* const position,
                                            IntrusiveListNode* const node);
    static void                 Remove(IntrusiveListNode* const node);

private:
    IntrusiveListNode           mHead;

};

#define INTRUSIVELIST_TEMPLATE \
    template <typename T, IntrusiveListNode T::*Member>

INTRUSIVELIST_TEMPLATE
inline IntrusiveList<T, Member>::IntrusiveList()
{
}

INTRUSIVELIST_TEMPLATE
inline IntrusiveList<T, Member>::IntrusiveList(const IntrusiveList& other)
{
}

INTRUSIVELIST_TEMPLATE
inline IntrusiveList<T, Member>::IntrusiveList(IntrusiveList&& other)
{
    mHead.mPrevious = other.mHead.mPrevious;
    mHead.mNext     = other.mHead.mNext;

    other.mHead.mPrevious = &other.mHead;
    other.mHead.mNext     = &other.mHead;
}

INTRUSIVELIST_TEMPLATE
inline IntrusiveList<T, Member>::~IntrusiveList()
{
    Assert(IsEmpty());
}

INTRUSIVELIST_TEMPLATE
inline IntrusiveList<T, Member>& IntrusiveList<T, Member>::operator=(const IntrusiveList& other)
{
}

INTRUSIVELIST_TEMPLATE
inline IntrusiveList<T, Member>& IntrusiveList<T, Member>::operator=(IntrusiveList&& other)
{
    Clear();

    mHead.mPrevious = other.mHead.mPrevious;
    mHead.mNext     = other.mHead.mNext;

    other.mHead.mPrevious = &other.mHead;
    other.mHead.mNext     = &other.mHead;
}

INTRUSIVELIST_TEMPLATE
inline bool IntrusiveList<T, Member>::IsEmpty() const
{
    return !mHead.IsInserted();
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::Append(T* const value)
{
    InsertBefore(&mHead, Impl::GetNode(value));
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::Prepend(T* const value)
{
    InsertAfter(&mHead, Impl::GetNode(value));
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::InsertBefore(T* const position, T* const value)
{
    InsertBefore(Impl::GetNode(position), Impl::GetNode(value));
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::InsertAfter(T* const position, T* const value)
{
    InsertAfter(Impl::GetNode(position), Impl::GetNode(value));
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::Remove(T* const value)
{
    Remove(Impl::GetNode(value));
}

INTRUSIVELIST_TEMPLATE
inline T* IntrusiveList<T, Member>::RemoveFirst()
{
    T* const value = First();

    if (value)
    {
        Remove(value);
    }

    return value;
}

INTRUSIVELIST_TEMPLATE
inline T* IntrusiveList<T, Member>::RemoveLast()
{
    T* const value = Last();

    if (value)
    {
        Remove(value);
    }

    return value;
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::Clear()
{
    while (!IsEmpty())
    {
        RemoveFirst();
    }
}

INTRUSIVELIST_TEMPLATE
inline T* IntrusiveList<T, Member>::First() const
{
    if (!IsEmpty())
    {
        return Impl::GetValue(mHead.mNext);
    }
    else
    {
        return nullptr;
    }
}

INTRUSIVELIST_TEMPLATE
inline T* IntrusiveList<T, Member>::Last() const
{
    if (!IsEmpty())
    {
        return Impl::GetValue(mHead.mPrevious);
    }
    else
    {
        return nullptr;
    }
}

INTRUSIVELIST_TEMPLATE
inline T* IntrusiveList<T, Member>::Previous(T* const current) const
{
    IntrusiveListNode* node = Impl::GetNode(current);

    if (node->mPrevious != &mHead)
    {
        return Impl::GetValue(node->mPrevious);
    }
    else
    {
        return nullptr;
    }
}

INTRUSIVELIST_TEMPLATE
inline const T* IntrusiveList<T, Member>::Previous(const T* const current) const
{
    IntrusiveListNode* node = Impl::GetNode(current);

    if (node->mPrevious != &mHead)
    {
        return Impl::GetValue(node->mPrevious);
    }
    else
    {
        return nullptr;
    }
}

INTRUSIVELIST_TEMPLATE
inline T* IntrusiveList<T, Member>::Next(T* const current) const
{
    IntrusiveListNode* node = Impl::GetNode(current);

    if (node->mNext != &mHead)
    {
        return Impl::GetValue(node->mNext);
    }
    else
    {
        return nullptr;
    }
}

INTRUSIVELIST_TEMPLATE
inline const T* IntrusiveList<T, Member>::Next(const T* const current) const
{
    IntrusiveListNode* node = Impl::GetNode(current);

    if (node->mNext != &mHead)
    {
        return Impl::GetValue(node->mNext);
    }
    else
    {
        return nullptr;
    }
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::InsertBefore(IntrusiveListNode* const position,
                                                   IntrusiveListNode* const node)
{
    Assert(!node->IsInserted());

    node->mPrevious = position->mPrevious;
    node->mNext     = position;

    position->mPrevious->mNext = node;
    position->mPrevious        = node;
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::InsertAfter(IntrusiveListNode* const position,
                                                  IntrusiveListNode* const node)
{
    Assert(!node->IsInserted());

    node->mPrevious = position;
    node->mNext     = position->mNext;

    position->mNext->mPrevious = node;
    position->mNext            = node;
}

INTRUSIVELIST_TEMPLATE
inline void IntrusiveList<T, Member>::Remove(IntrusiveListNode* const node)
{
    Assert(node->IsInserted());

    node->mPrevious->mNext = node->mNext;
    node->mNext->mPrevious = node->mPrevious;

    node->mNext     = node;
    node->mPrevious = node;
}

#undef INTRUSIVELIST_TEMPLATE
