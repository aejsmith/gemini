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

#include "Core/Math.h"

template <typename T>
struct RectImpl
{
    T                           x;
    T                           y;
    T                           width;
    T                           height;

public:
    using VecType             = glm::tvec2<T, glm::highp>;

public:
                                RectImpl();

                                RectImpl(const T inX,
                                         const T inY,
                                         const T inWidth,
                                         const T inHeight);

                                RectImpl(const VecType& position,
                                         const VecType& size);

public:
    VecType                     GetPosition() const     { return VecType(x, y); }
    VecType                     GetSize() const         { return VecType(width, height); }

    /**
     * Check whether a point or another rectangle is completely within this one.
     */
    bool                        Contains(const VecType& point) const;
    bool                        Contains(const RectImpl& other) const;

    bool                        operator ==(const RectImpl& other) const;
    bool                        operator !=(const RectImpl& other) const;
};

using Rect    = RectImpl<float>;
using IntRect = RectImpl<int32_t>;

template <typename T>
inline RectImpl<T>::RectImpl() :
    x       (0),
    y       (0),
    width   (0),
    height  (0)
{
}

template <typename T>
inline RectImpl<T>::RectImpl(const T inX,
                             const T inY,
                             const T inWidth,
                             const T inHeight) :
    x       (inX),
    y       (inY),
    width   (inWidth),
    height  (inHeight)
{
}

template <typename T>
inline RectImpl<T>::RectImpl(const VecType& position,
                             const VecType& size) :
    x       (position.x),
    y       (position.y),
    width   (size.x),
    height  (size.y)
{
}

template <typename T>
inline bool RectImpl<T>::Contains(const VecType& point) const
{
    return point.x >= x &&
           point.y >= y &&
           point.x < (x + width) &&
           point.y < (y + height);
}

template <typename T>
inline bool RectImpl<T>::Contains(const RectImpl& other) const
{
    return other.x >= x &&
           other.y >= y &&
           (other.x + other.width) <= (x + width) &&
           (other.y + other.height) <= (y + height);
}

template <typename T>
inline bool RectImpl<T>::operator ==(const RectImpl& other) const
{
    return x == other.x &&
           y == other.y &&
           width == other.width &&
           height == other.height;
}

template <typename T>
inline bool RectImpl<T>::operator !=(const RectImpl& other) const
{
    return !(*this == other);
}
