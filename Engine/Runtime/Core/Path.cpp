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

#include "Core/Path.h"

static const char kPlatformPathSeparator = '/';

size_t Path::CountComponents() const
{
    /* We always have at least one component. Each '/' adds another. */
    size_t count = 1;

    /* Start at position 1, we don't want to count an initial '/' (i.e.
     * absolute path) as another component. */
    for (size_t pos = 1; pos < mPath.length(); pos++)
    {
        if (mPath[pos] == '/')
            count++;
    }

    return count;
}

Path Path::Subset(const size_t inFirstComponent,
                  const size_t inCount) const
{
    if (inCount == 0)
        return Path();

    size_t current = 0;
    size_t start   = 0;

    for (size_t pos = 1; pos < mPath.length(); pos++)
    {
        if (mPath[pos] == '/')
        {
            current++;

            if (current == inFirstComponent)
                start = pos + 1;

            /* Don't allow it to overflow. */
            if (inFirstComponent + inCount > inFirstComponent && current == inFirstComponent + inCount)
            {
                return Path(mPath.substr(start,
                                         pos - start),
                            kNormalized);
            }
        }
    }

    return (current >= inFirstComponent)
               ? Path(mPath.substr(start),
                      kNormalized)
               : Path();
}

std::string Path::ToPlatform() const
{
    std::string ret = mPath;
    std::replace(ret.begin(), ret.end(), '/', kPlatformPathSeparator);
    return ret;
}

Path& Path::operator /=(const Path& inOther)
{
    if (inOther.IsAbsolute())
    {
        mPath = inOther.mPath;
        return *this;
    }
    else if (inOther.IsRoot())
    {
        return *this;
    }

    if (&inOther == this)
    {
        std::string copy(inOther.GetString());
        mPath += '/';
        mPath += copy;
    }
    else
    {
        if (IsRoot())
        {
            mPath = inOther.mPath;
        }
        else
        {
            if (!IsAbsoluteRoot())
                mPath += '/';

            mPath += inOther.mPath;
        }
    }

    return *this;
}

bool Path::IsRoot() const
{
    return mPath.length() == 1 && mPath[0] == '.';
}

bool Path::IsAbsoluteRoot() const
{
    #if GEMINI_PLATFORM_WIN32
        return mPath.length() == 3 && mPath[1] == ':' && mPath[2] == '/';
    #else
        return mPath.length() == 1 && mPath[0] == '/';
    #endif
}

bool Path::IsRelative() const
{
    return !IsAbsolute();
}

bool Path::IsAbsolute() const
{
    #if GEMINI_PLATFORM_WIN32
        return mPath.length() >= 3 && mPath[1] == ':' && mPath[2] == '/';
    #else
        return mPath[0] == '/';
    #endif
}

Path Path::GetDirectoryName() const
{
    const size_t pos = mPath.rfind('/');

    if (pos == std::string::npos)
    {
        return Path();
    }
    else if (pos == 0)
    {
        return Path("/", kNormalized);
    }
    else
    {
        return Path(mPath.substr(0, pos), kNormalized);
    }
}

Path Path::GetFileName() const
{
    const size_t pos = mPath.rfind('/');

    if (pos == std::string::npos || (pos == 0 && mPath.length() == 1))
    {
        return *this;
    }
    else
    {
        return Path(mPath.substr(pos + 1), kNormalized);
    }
}

std::string Path::GetBaseFileName() const
{
    const Path file  = GetFileName();
    const size_t pos = file.mPath.rfind('.');

    if (pos == 0 || pos == std::string::npos)
    {
        return file.mPath;
    }
    else
    {
        return file.mPath.substr(0, pos);
    }
}

std::string Path::GetExtension(const bool inKeepDot) const
{
    const Path file  = GetFileName();
    const size_t pos = file.mPath.rfind('.');

    if (pos == 0 || pos == std::string::npos)
    {
        return std::string();
    }
    else
    {
        return file.mPath.substr((inKeepDot) ? pos : pos + 1);
    }
}

void Path::Normalize(const char*              inPath,
                     const size_t             inLength,
                     const NormalizationState inState,
                     std::string&             outNormalized)
{
    if (inLength == 0 || (inLength == 1 && inPath[0] == '.'))
    {
        outNormalized = '.';
        return;
    }

    outNormalized.reserve(inLength);

    bool seenSlash = false;
    bool seenDot   = false;

    for (size_t pos = 0; pos < inLength; pos++)
    {
        char ch = inPath[pos];

        if (inState == kUnnormalizedPlatform)
        {
            /* Convert platform-specific path separators. */
            if (ch == kPlatformPathSeparator)
                ch = '/';
        }

        if (ch == '/')
        {
            if (seenDot)
            {
                seenDot = false;
            }
            else if (!seenSlash)
            {
                outNormalized.push_back('/');
            }

            seenSlash = true;
        }
        else if (ch == '.' && (seenSlash || pos == 0))
        {
            seenDot = true;
            seenSlash = false;
        }
        else
        {
            if (seenDot)
            {
                outNormalized.push_back('.');
                seenDot = false;
            }

            seenSlash = false;

            outNormalized.push_back(ch);
        }
    }

    /* Ensure we haven't written an extraneous '/' at the end. */
    if (outNormalized.length() > 1 && outNormalized[outNormalized.length() - 1] == '/')
        outNormalized.pop_back();

    outNormalized.shrink_to_fit();
}
