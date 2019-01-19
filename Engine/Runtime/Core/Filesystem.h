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

/**
 * Right now this is just a wrapper for a platform-dependent filesystem
 * implementation. Relative paths are relative to the game base directory.
 *
 * In future, when we support some sort of data packages, we will implement a
 * layered system where package files will be layered on top of the base FS.
 * This would cause relative paths to be resolved into the package files, but
 * absolute paths (for example for user data) would be passed down to the
 * underlying platform FS. Multiple packages will be able to be layered on top
 * of each other, so for example patches could be distributed as a package that
 * only changes the necessary files which would be layered onto the base
 * package.
 */

#pragma once

#include "Core/DataStream.h"
#include "Core/Path.h"
#include "Core/Utility.h"

#include <algorithm>

enum FileType
{
    kFileType_File,
    kFileType_Directory,
    kFileType_Other,
};

enum FileMode
{
    kFileMode_Read     = (1 << 0),       /**< Open for reading. */
    kFileMode_Write    = (1 << 1),       /**< Open for writing. */
    kFileMode_Create   = (1 << 2),       /**< Create the file if it doesn't exist (use with kWrite). */
    kFileMode_Truncate = (1 << 3),       /**< Truncate the file if it already exists. */
};

DEFINE_ENUM_BITWISE_OPS(FileMode);

/** A handle to a regular file allowing I/O on the file. */
class File : public DataStream, Uncopyable
{
protected:
                                File() {}

};

/** A handle to a directory allowing the directory contents to be iterated. */
class Directory : Uncopyable
{
public:
    struct Entry
    {
        Path                    name;
        FileType                type;
    };

public:
    virtual                     ~Directory() {}

    /** Reset the directory to the beginning. */
    virtual void                Reset() = 0;

    /**
     * Gets the next directory entry. This API ignores '.' and '..' entries.
     * Returns false if the end of the directory has been reached or an error
     * occurred.
     */
    virtual bool                Next(Entry& outEntry) = 0;

protected:
                                Directory() {}

};

/**
 * These functions provides an interface to access the filesystem. We use a
 * standard path format across all platforms, using '/' as the path separator.
 * Absolute paths always refer to the underlying system FS. However, relative
 * paths into the engine base directory can resolve into package files instead.
 * Whether a package file or the system FS is used is transparent to users of
 * this class.
 */
namespace Filesystem
{
    extern File*                OpenFile(const Path&    inPath,
                                         const FileMode inMode = kFileMode_Read);

    extern Directory*           OpenDirectory(const Path& inPath);

    extern bool                 Exists(const Path& inPath);

    extern bool                 IsType(const Path&    inPath,
                                       const FileType inType);

    extern bool                 SetWorkingDirectory(const Path& inPath);

    extern bool                 GetFullPath(const Path& inPath,
                                            Path&       outFullPath);

}
