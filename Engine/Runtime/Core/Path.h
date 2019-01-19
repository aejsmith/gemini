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

#include "Core/String.h"

#include <algorithm>
#include <cstring>

/**
 * This class stores a path string in a standard format, using '/' as the path
 * separator. Paths stored by this class are always normalized, i.e. extraneous
 * separators are removed, as are components that are just '.'.
 */
class Path
{
public:
    enum NormalizationState
    {
        /** Already normalized. */
        kNormalized,

        /** Unnormalized. */
        kUnnormalized,

        /** Unnormalized, in platform-specific format (i.e. different separators). */
        kUnnormalizedPlatform,
    };

public:
                                    Path();
                                    Path(const Path& inOther);
                                    Path(Path&& inOther);
                                    Path(const std::string&       inPath,
                                         const NormalizationState inState = kUnnormalized);
                                    Path(const char*              inPath,
                                         const NormalizationState inState = kUnnormalized);

public:
    Path&                           operator =(const Path& inOther);
    Path&                           operator =(Path&& inOther);

    bool                            operator ==(const Path& inOther) const;

    const std::string&              GetString() const       { return mPath; }
    const char*                     GetCString() const      { return mPath.c_str(); }

    /**
     * Modifiers.
     */

    /**
     * Get a subset of the path, starting from the specified component and
     * including the number of components given. If first + count is greater
     * than the number of components, the returned path will include up to the
     * end.
     */
    Path                            Subset(const size_t inFirstComponent,
                                           const size_t inCount = std::numeric_limits<size_t>::max()) const;

    /** Convert to a platform-specific representation of the path. */
    std::string                     ToPlatform() const;

    /**
     * Append another path to this path, with a separator between them. If the
     * second path is an absolute path, it will replace this one.
     */
    Path&                           operator /=(const Path& inOther);

    /**
     * Concatenate two paths, with a separator between them. If the other path
     * is an absolute path, the returned path will be just that one.
     */
    Path                            operator /(const Path& inOther) const;

    /** Append a string to the path, not accounting for separators. */
    Path&                           operator +=(const std::string& inString);

    /** Concatenate two path strings, not accounting for separators. */
    Path                            operator +(const std::string& inString) const;

    /**
     * Queries.
     */

    size_t                          CountComponents() const;

    /** Return whether the path refers to the engine root. */
    bool                            IsRoot() const;

    /** Return whether the path refers to the absolute filesystem root. */
    bool                            IsAbsoluteRoot() const;

    bool                            IsRelative() const;
    bool                            IsAbsolute() const;

    /** Get the directory name (all but last component). */
    Path                            GetDirectoryName() const;

    /** Get the file name (last component). */
    Path                            GetFileName() const;

    /** Separate into base (without extension) and extension. */
    std::string                     GetBaseFileName() const;
    std::string                     GetExtension(const bool inKeepDot = false) const;

private:
    static void                     Normalize(const char*              inPath,
                                              const size_t             inLength,
                                              const NormalizationState inState,
                                              std::string&             outNormalized);

private:
    std::string                     mPath;

};

inline Path::Path() :
    mPath   (".")
{
}

inline Path::Path(const Path& inOther) :
    mPath   (inOther.mPath)
{
}

inline Path::Path(Path&& inOther) :
    mPath   (std::move(inOther.mPath))
{
}

inline Path::Path(const std::string&       inPath,
                  const NormalizationState inState)
{
    if (inState == kNormalized)
    {
        mPath = inPath;
    }
    else
    {
        Normalize(inPath.c_str(),
                  inPath.length(),
                  inState,
                  mPath);
    }
}

inline Path::Path(const char*              inPath,
                  const NormalizationState inState)
{
    if (inState == kNormalized)
    {
        mPath = inPath;
    }
    else
    {
        Normalize(inPath,
                  strlen(inPath),
                  inState,
                  mPath);
    }
}

inline Path& Path::operator =(const Path& inOther)
{
    mPath = inOther.mPath;
    return *this;
}

inline Path& Path::operator =(Path&& inOther)
{
    mPath = std::move(inOther.mPath);
    return *this;
}

inline bool Path::operator ==(const Path& inOther) const
{
    return mPath == inOther.mPath;
}

inline Path Path::operator /(const Path& inOther) const
{
    if (inOther.IsAbsolute())
    {
        return inOther;
    }
    else
    {
        return Path(*this) /= inOther;
    }
}

inline Path& Path::operator +=(const std::string& inString)
{
    mPath += inString;
    return *this;
}

inline Path Path::operator +(const std::string& inString) const
{
    return Path(*this) += inString;
}
