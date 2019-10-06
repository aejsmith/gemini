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

#include "Core/Base64.h"

#include "Core/Utility.h"

static constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr char kBase64Pad     = '=';

static inline bool IsBase64(const char inChar)
{
    return isalnum(inChar) || inChar == '+' || inChar == '/';
}

static inline uint8_t GetIndex(const char inChar)
{
    if (inChar == kBase64Pad)
    {
        return 0;
    }

    for (uint8_t i = 0; i < ArraySize(kBase64Chars); i++)
    {
        if (kBase64Chars[i] == inChar)
        {
            return i;
        }
    }

    Unreachable();
}

bool Base64::Decode(const char* const inString,
                    const size_t      inLength,
                    ByteArray&        outData)
{
    if (inLength % 4)
    {
        return false;
    }

    /* Actual size may be smaller due to padding, we'll shrink at the end. */
    const size_t maximumLength = (inLength / 4) * 3;
    size_t actualLength        = 0;

    ByteArray result(maximumLength);

    const char* string = inString;

    size_t remaining = inLength;
    size_t charCount = 0;
    size_t padCount  = 0;

    while (remaining > 0)
    {
        if (string[charCount] == kBase64Pad)
        {
            if (++padCount > 2)
            {
                return false;
            }
        }
        else if (padCount || !IsBase64(string[charCount]))
        {
            return false;
        }

        if (++charCount == 4)
        {
            const uint8_t chars[4] =
            {
                GetIndex(string[0]),
                GetIndex(string[1]),
                GetIndex(string[2]),
                GetIndex(string[3]),
            };

            result[actualLength    ] =  (chars[0]        << 2) | ((chars[1] & 0x30) >> 4);
            result[actualLength + 1] = ((chars[1] & 0xf) << 4) | ((chars[2] & 0x3c) >> 2);
            result[actualLength + 2] = ((chars[2] & 0x3) << 6) |   chars[3];

            actualLength += 3 - padCount;

            string += charCount;
            charCount = 0;
        }

        remaining--;
    }

    result.Resize(actualLength, false);

    outData = result;
    return true;
}

bool Base64::Decode(const std::string& inString,
                    ByteArray&         outData)
{
    return Decode(inString.c_str(), inString.length(), outData);
}

std::string Base64::Encode(const void* const inData,
                           const size_t      inLength)
{
    const size_t outputLength = 4 * ((RoundUp(inLength, size_t(3))) / 3);

    std::string result;
    result.reserve(outputLength);

    auto data = reinterpret_cast<const uint8_t*>(inData);

    size_t remaining = inLength;
    size_t byteCount = 0;

    while (remaining > 0)
    {
        if (++byteCount == 3)
        {
            result += kBase64Chars[ (data[0] & 0xfc) >> 2];
            result += kBase64Chars[((data[0] & 0x03) << 4) | ((data[1] & 0xf0) >> 4)];
            result += kBase64Chars[((data[1] & 0x0f) << 2) | ((data[2] & 0xc0) >> 6)];
            result += kBase64Chars[  data[2] & 0x3f];

            data += byteCount;
            byteCount = 0;
        }

        remaining--;
    }

    /* Anything left over that we haven't written yet needs padding. */
    if (byteCount > 0)
    {
        const uint8_t bytes[3] =
        {
            data[0],
            (byteCount == 2) ? data[1] : uint8_t(0),
            uint8_t(0),
        };

        result +=                    kBase64Chars[ (bytes[0] & 0xfc) >> 2];
        result +=                    kBase64Chars[((bytes[0] & 0x03) << 4) | ((bytes[1] & 0xf0) >> 4)];
        result += (byteCount == 2) ? kBase64Chars[((bytes[1] & 0x0f) << 2) | ((bytes[2] & 0xc0) >> 6)] : kBase64Pad;
        result +=                    kBase64Pad;
    }

    return result;
}

std::string Base64::Encode(const ByteArray& inData)
{
    return Encode(inData.Get(), inData.GetSize());
}
