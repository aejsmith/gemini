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

/**
 * Helper functions for implementation of intrusive containers, where the link
 * to the container is embedded within the objects being stored in the container
 * rather than allocated separately.
 */
template <typename T, typename Node, Node T::*Member>
class IntrusiveImpl
{
public:
    /** Given a node pointer, return a pointer to the object containing it. */
    static T*               GetValue(Node* const node);
    static const T*         GetValue(const Node* const node);

    /** Given a value pointer, return a pointer to its node. */
    static Node*            GetNode(T* const value);
    static const Node*      GetNode(const T* const value);
};

template <typename T, typename Node, Node T::*Member>
inline T* IntrusiveImpl<T, Node, Member>::GetValue(Node* const node)
{
    Node* const offset = &(reinterpret_cast<T*>(0)->*Member);
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(offset));
}

template <typename T, typename Node, Node T::*Member>
inline const T* IntrusiveImpl<T, Node, Member>::GetValue(const Node* const node)
{
    return const_cast<const T*>(GetValue(const_cast<Node*>(node)));
}

template <typename T, typename Node, Node T::*Member>
inline Node* IntrusiveImpl<T, Node, Member>::GetNode(T* const value)
{
    return &(value->*Member);
}

template <typename T, typename Node, Node T::*Member>
inline const Node* IntrusiveImpl<T, Node, Member>::GetNode(const T* const value)
{
    return &(value->*Member);
}
