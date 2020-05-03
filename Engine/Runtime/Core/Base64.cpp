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

#include "Core/Base64.h"

#include "Core/Utility.h"

static constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr char kBase64Pad     = '=';

static inline bool IsBase64(const char ch)
{
    return isalnum(ch) || ch == '+' || ch == '/';
}

static inline uint8_t GetIndex(const char ch)
{
    if (ch == kBase64Pad)
    {
        return 0;
    }

    for (uint8_t i = 0; i < ArraySize(kBase64Chars); i++)
    {
        if (kBase64Chars[i] == ch)
        {
            return i;
        }
    }

    Unreachable();
}

bool Base64::Decode(const char* const string,
                    const size_t      length,
                    ByteArray&        outData)
{
    if (length % 4)
    {
        return false;
    }

    /* Actual size may be smaller due to padding, we'll shrink at the end. */
    const size_t maximumLength = (length / 4) * 3;
    size_t actualLength        = 0;

    ByteArray result(maximumLength);

    const char* pos = string;

    size_t remaining = length;
    size_t charCount = 0;
    size_t padCount  = 0;

    while (remaining > 0)
    {
        if (pos[charCount] == kBase64Pad)
        {
            if (++padCount > 2)
            {
                return false;
            }
        }
        else if (padCount || !IsBase64(pos[charCount]))
        {
            return false;
        }

        if (++charCount == 4)
        {
            const uint8_t chars[4] =
            {
                GetIndex(pos[0]),
                GetIndex(pos[1]),
                GetIndex(pos[2]),
                GetIndex(pos[3]),
            };

            result[actualLength    ] =  (chars[0]        << 2) | ((chars[1] & 0x30) >> 4);
            result[actualLength + 1] = ((chars[1] & 0xf) << 4) | ((chars[2] & 0x3c) >> 2);
            result[actualLength + 2] = ((chars[2] & 0x3) << 6) |   chars[3];

            actualLength += 3 - padCount;

            pos += charCount;
            charCount = 0;
        }

        remaining--;
    }

    result.Resize(actualLength, false);

    outData = result;
    return true;
}

bool Base64::Decode(const std::string& string,
                    ByteArray&         outData)
{
    return Decode(string.c_str(), string.length(), outData);
}

std::string Base64::Encode(const void* const data,
                           const size_t      length)
{
    const size_t outputLength = 4 * ((RoundUp(length, size_t(3))) / 3);

    std::string result;
    result.reserve(outputLength);

    auto pos = reinterpret_cast<const uint8_t*>(data);

    size_t remaining = length;
    size_t byteCount = 0;

    while (remaining > 0)
    {
        if (++byteCount == 3)
        {
            result += kBase64Chars[ (pos[0] & 0xfc) >> 2];
            result += kBase64Chars[((pos[0] & 0x03) << 4) | ((pos[1] & 0xf0) >> 4)];
            result += kBase64Chars[((pos[1] & 0x0f) << 2) | ((pos[2] & 0xc0) >> 6)];
            result += kBase64Chars[  pos[2] & 0x3f];

            pos += byteCount;
            byteCount = 0;
        }

        remaining--;
    }

    /* Anything left over that we haven't written yet needs padding. */
    if (byteCount > 0)
    {
        const uint8_t bytes[3] =
        {
            pos[0],
            (byteCount == 2) ? pos[1] : uint8_t(0),
            uint8_t(0),
        };

        result +=                    kBase64Chars[ (bytes[0] & 0xfc) >> 2];
        result +=                    kBase64Chars[((bytes[0] & 0x03) << 4) | ((bytes[1] & 0xf0) >> 4)];
        result += (byteCount == 2) ? kBase64Chars[((bytes[1] & 0x0f) << 2) | ((bytes[2] & 0xc0) >> 6)] : kBase64Pad;
        result +=                    kBase64Pad;
    }

    return result;
}

std::string Base64::Encode(const ByteArray& data)
{
    return Encode(data.Get(), data.GetSize());
}
