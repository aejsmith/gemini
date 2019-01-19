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

#include "Core/String.h"

#include <cstdio>

std::string StringUtils::VFormat(const char* const inFormat,
                                 va_list           inArgs)
{
    char buf[8192];
    vsnprintf(buf, 8192, inFormat, inArgs);
    return std::string(buf);
}

std::string StringUtils::Format(const char* inFormat,
                                ...)
{
    char buf[8192];
    va_list args;

    va_start(args, inFormat);
    vsnprintf(buf, 8192, inFormat, args);
    va_end(args);

    return std::string(buf);
}
