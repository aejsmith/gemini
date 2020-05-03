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

class Line
{
public:
                                Line();
                                Line(const glm::vec3& start,
                                     const glm::vec3& end);

    const glm::vec3&            GetStart() const    { return mStart; }
    const glm::vec3&            GetEnd() const      { return mEnd; }

private:
    glm::vec3                   mStart;
    glm::vec3                   mEnd;

};

inline Line::Line() :
    mStart  (0.0f, 0.0f, 0.0f),
    mEnd    (0.0f, 0.0f, 0.0f)
{
}

inline Line::Line(const glm::vec3& start,
                  const glm::vec3& end) :
    mStart  (start),
    mEnd    (end)
{
}
