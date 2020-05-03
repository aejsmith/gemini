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
                                    Path(const Path& other);
                                    Path(Path&& other);
                                    Path(const std::string&       path,
                                         const NormalizationState state = kUnnormalized);
                                    Path(const char*              path,
                                         const NormalizationState state = kUnnormalized);

public:
    Path&                           operator =(const Path& other);
    Path&                           operator =(Path&& other);

    bool                            operator ==(const Path& other) const;

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
    Path                            Subset(const size_t firstComponent,
                                           const size_t incount = std::numeric_limits<size_t>::max()) const;

    /** Convert to a platform-specific representation of the path. */
    std::string                     ToPlatform() const;

    /**
     * Append another path to this path, with a separator between them. If the
     * second path is an absolute path, it will replace this one.
     */
    Path&                           operator /=(const Path& other);

    /**
     * Concatenate two paths, with a separator between them. If the other path
     * is an absolute path, the returned path will be just that one.
     */
    Path                            operator /(const Path& other) const;

    /** Append a string to the path, not accounting for separators. */
    Path&                           operator +=(const std::string& string);

    /** Concatenate two path strings, not accounting for separators. */
    Path                            operator +(const std::string& string) const;

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
    std::string                     GetExtension(const bool keepDot = false) const;

private:
    static void                     Normalize(const char*              path,
                                              const size_t             length,
                                              const NormalizationState state,
                                              std::string&             outNormalized);

private:
    std::string                     mPath;

};

inline Path::Path() :
    mPath   (".")
{
}

inline Path::Path(const Path& other) :
    mPath   (other.mPath)
{
}

inline Path::Path(Path&& other) :
    mPath   (std::move(other.mPath))
{
}

inline Path::Path(const std::string&       path,
                  const NormalizationState state)
{
    if (state == kNormalized)
    {
        mPath = path;
    }
    else
    {
        Normalize(path.c_str(),
                  path.length(),
                  state,
                  mPath);
    }
}

inline Path::Path(const char*              path,
                  const NormalizationState state)
{
    if (state == kNormalized)
    {
        mPath = path;
    }
    else
    {
        Normalize(path,
                  strlen(path),
                  state,
                  mPath);
    }
}

inline Path& Path::operator =(const Path& other)
{
    mPath = other.mPath;
    return *this;
}

inline Path& Path::operator =(Path&& other)
{
    mPath = std::move(other.mPath);
    return *this;
}

inline bool Path::operator ==(const Path& other) const
{
    return mPath == other.mPath;
}

inline Path Path::operator /(const Path& other) const
{
    if (other.IsAbsolute())
    {
        return other;
    }
    else
    {
        return Path(*this) /= other;
    }
}

inline Path& Path::operator +=(const std::string& string)
{
    mPath += string;
    return *this;
}

inline Path Path::operator +(const std::string& string) const
{
    return Path(*this) += string;
}
