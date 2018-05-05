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

#include "Core/Hash.h"

#include <string>

inline size_t HashValue(const std::string& inValue)
{
    return HashData(inValue.c_str(), inValue.length());
}

namespace StringUtils
{
    /** Splits a string into tokens. */
    template <typename Container>
    void                        Tokenize(const std::string& inString,
                                         Container&         outTokens,
                                         const char*        inDelimiters = " ",
                                         const int          inMaxTokens  = -1,
                                         const bool         inTrimEmpty  = true);

    /** Format a string as per printf(). */
    std::string                 VFormat(const char* const inFormat,
                                        va_list           inArgs);

    /** Format a string as per printf(). */
    std::string                 Format(const char* const inFormat,
                                       ...);

};

template <typename Container>
inline void StringUtils::Tokenize(const std::string& inString,
                                  Container&         outTokens,
                                  const char*        inDelimiters,
                                  const int          inMaxTokens,
                                  const bool         inTrimEmpty)
{
    size_t last   = 0;
    size_t pos    = 0;
    int numTokens = 0;

    while (pos != std::string::npos)
    {
        if (inMaxTokens > 0 && numTokens == inMaxTokens - 1)
        {
            outTokens.emplace_back(inString, last);
            break;
        }
        else
        {
            pos = inString.find_first_of(inDelimiters, last);

            if (!inTrimEmpty || last != ((pos == std::string::npos) ? inString.length() : pos))
                outTokens.emplace_back(inString, last, pos - last);

            last = pos + 1;
            numTokens++;
        }
    }
}
