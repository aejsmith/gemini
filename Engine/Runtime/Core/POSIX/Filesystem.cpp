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

#include "Core/Filesystem.h"

#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

class POSIXFile final : public File
{
public:
                                POSIXFile(const int inFD);
                                ~POSIXFile();

    uint64_t                    GetSize() const override;
    bool                        Read(void* const outBuffer, const size_t inSize) override;
    bool                        Write(const void* const inBuffer, const size_t inSize) override;
    bool                        Seek(const SeekMode inMode, const int64_t inOffset) override;
    uint64_t                    GetOffset() const override;

    bool                        Read(void* const    outBuffer,
                                     const size_t   inSize,
                                     const uint64_t inOffset) override;

    bool                        Write(const void* const inBuffer,
                                      const size_t      inSize,
                                      const uint64_t    inOffset) override;

private:
    int                         mFD;

};

class POSIXDirectory final : public Directory
{
public:
                                POSIXDirectory(DIR* const inDirectory);
                                ~POSIXDirectory();

    void                        Reset() override;
    bool                        Next(Entry& outEntry) override;

private:
    DIR*                        mDirectory;

};

POSIXFile::POSIXFile(const int inFD) :
    mFD (inFD)
{
}

POSIXFile::~POSIXFile()
{
    close(mFD);
}

uint64_t POSIXFile::GetSize() const
{
    struct stat st;

    const int ret = fstat(mFD, &st);
    if (ret != 0)
    {
        return 0;
    }

    return st.st_size;
}

bool POSIXFile::Read(void* const outBuffer, const size_t inSize)
{
    return read(mFD, outBuffer, inSize) == static_cast<ssize_t>(inSize);
}

bool POSIXFile::Write(const void* const inBuffer, const size_t inSize)
{
    return write(mFD, inBuffer, inSize) == static_cast<ssize_t>(inSize);
}

bool POSIXFile::Seek(const SeekMode inMode, const int64_t inOffset)
{
    int whence;

    switch (inMode)
    {
        case kSeekMode_Set:
            whence = SEEK_SET;
            break;

        case kSeekMode_Current:
            whence = SEEK_CUR;
            break;

        case kSeekMode_End:
            whence = SEEK_END;
            break;

        default:
            return false;

    }

    return lseek(mFD, inOffset, whence) == 0;
}

uint64_t POSIXFile::GetOffset() const
{
    return lseek(mFD, SEEK_CUR, 0);
}

bool POSIXFile::Read(void* const    outBuffer,
                     const size_t   inSize,
                     const uint64_t inOffset)
{
    return pread(mFD, outBuffer, inSize, inOffset) == static_cast<ssize_t>(inSize);
}

/** Write to the file at the specified offset.
 * @param buf           Buffer containing data to write.
 * @param size          Number of bytes to write.
 * @param offset        Offset to write to.
 * @return              Whether the write was successful. */
bool POSIXFile::Write(const void* const inBuffer,
                      const size_t      inSize,
                      const uint64_t    inOffset)
{
    return pwrite(mFD, inBuffer, inSize, inOffset) == static_cast<ssize_t>(inSize);
}

POSIXDirectory::POSIXDirectory(DIR* const inDirectory) :
    mDirectory (inDirectory)
{
}

POSIXDirectory::~POSIXDirectory()
{
    closedir(mDirectory);
}

void POSIXDirectory::Reset()
{
    rewinddir(mDirectory);
}

bool POSIXDirectory::Next(Entry& outEntry)
{
    struct dirent* dent;

    while (true)
    {
        dent = readdir(mDirectory);
        if (!dent)
        {
            return false;
        }

        if (strcmp(dent->d_name, ".") && strcmp(dent->d_name, ".."))
        {
            break;
        }
    }

    outEntry.name = dent->d_name;

    switch (dent->d_type)
    {
        case DT_REG:
            outEntry.type = kFileType_File;
            break;

        case DT_DIR:
            outEntry.type = kFileType_Directory;
            break;

        default:
            outEntry.type = kFileType_Other;
            break;

    }

    return true;
}

File* Filesystem::OpenFile(const Path&    inPath,
                           const FileMode inMode) {
    int flags = 0;

    if (inMode & kFileMode_Read)
    {
        flags |= O_RDONLY;
    }

    if (inMode & kFileMode_Write)
    {
        flags |= O_WRONLY;
    }

    if (inMode & kFileMode_Create)
    {
        Assert(inMode & kFileMode_Write);

        flags |= O_CREAT;
    }

    if (inMode & kFileMode_Truncate)
    {
        flags |= O_TRUNC;
    }

    const int fd = open(inPath.GetCString(), flags, 0644);
    if (fd < 0)
    {
        return nullptr;
    }

    return new POSIXFile(fd);
}

Directory* Filesystem::OpenDirectory(const Path& inPath)
{
    DIR* directory = opendir(inPath.GetCString());
    if (!directory)
    {
        return nullptr;
    }

    return new POSIXDirectory(directory);
}

bool Filesystem::Exists(const Path& inPath)
{
    struct stat st;
    return stat(inPath.GetCString(), &st) == 0;
}

bool Filesystem::IsType(const Path&    inPath,
                        const FileType inType)
{
    struct stat st;
    if (stat(inPath.GetCString(), &st) != 0)
        return false;

    switch (inType)
    {
        case kFileType_File:
            return S_ISREG(st.st_mode);

        case kFileType_Directory:
            return S_ISDIR(st.st_mode);

        default:
            return true;

    }
}

bool Filesystem::SetWorkingDirectory(const Path& inPath)
{
    return chdir(inPath.GetCString()) == 0;
}

bool Filesystem::GetFullPath(const Path& inPath,
                             Path&       outFullPath)
{
    char* str = realpath(inPath.GetCString(), nullptr);
    if (!str)
    {
        return false;
    }

    outFullPath = Path(str, Path::kNormalized);
    free(str);
    return true;
}
