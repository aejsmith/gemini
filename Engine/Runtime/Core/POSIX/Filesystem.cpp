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

#include "Core/Filesystem.h"

#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

class POSIXFile final : public File
{
public:
                                POSIXFile(const int fd);
                                ~POSIXFile();

    uint64_t                    GetSize() const override;
    bool                        Read(void* const outBuffer, const size_t size) override;
    bool                        Write(const void* const buffer, const size_t size) override;
    bool                        Seek(const SeekMode mode, const int64_t offset) override;
    uint64_t                    GetOffset() const override;

    bool                        Read(void* const    outBuffer,
                                     const size_t   size,
                                     const uint64_t offset) override;

    bool                        Write(const void* const buffer,
                                      const size_t      size,
                                      const uint64_t    offset) override;

private:
    int                         mFD;

};

class POSIXDirectory final : public Directory
{
public:
                                POSIXDirectory(DIR* const directory);
                                ~POSIXDirectory();

    void                        Reset() override;
    bool                        Next(Entry& outEntry) override;

private:
    DIR*                        mDirectory;

};

POSIXFile::POSIXFile(const int fd) :
    mFD (fd)
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

bool POSIXFile::Read(void* const outBuffer, const size_t size)
{
    return read(mFD, outBuffer, size) == static_cast<ssize_t>(size);
}

bool POSIXFile::Write(const void* const buffer, const size_t size)
{
    return write(mFD, buffer, size) == static_cast<ssize_t>(size);
}

bool POSIXFile::Seek(const SeekMode mode, const int64_t offset)
{
    int whence;

    switch (mode)
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

    return lseek(mFD, offset, whence) == 0;
}

uint64_t POSIXFile::GetOffset() const
{
    return lseek(mFD, 0, SEEK_CUR);
}

bool POSIXFile::Read(void* const    outBuffer,
                     const size_t   size,
                     const uint64_t offset)
{
    return pread(mFD, outBuffer, size, offset) == static_cast<ssize_t>(size);
}

bool POSIXFile::Write(const void* const buffer,
                      const size_t      size,
                      const uint64_t    offset)
{
    return pwrite(mFD, buffer, size, offset) == static_cast<ssize_t>(size);
}

POSIXDirectory::POSIXDirectory(DIR* const directory) :
    mDirectory (directory)
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

File* Filesystem::OpenFile(const Path&    path,
                           const FileMode mode)
{
    int flags = 0;

    if (mode & kFileMode_Read)
    {
        flags |= O_RDONLY;
    }

    if (mode & kFileMode_Write)
    {
        flags |= O_WRONLY;
    }

    if (mode & kFileMode_Create)
    {
        Assert(mode & kFileMode_Write);

        flags |= O_CREAT;
    }

    if (mode & kFileMode_Truncate)
    {
        flags |= O_TRUNC;
    }

    const int fd = open(path.GetCString(), flags, 0644);
    if (fd < 0)
    {
        return nullptr;
    }

    return new POSIXFile(fd);
}

Directory* Filesystem::OpenDirectory(const Path& path)
{
    DIR* directory = opendir(path.GetCString());
    if (!directory)
    {
        return nullptr;
    }

    return new POSIXDirectory(directory);
}

bool Filesystem::Exists(const Path& path)
{
    struct stat st;
    return stat(path.GetCString(), &st) == 0;
}

bool Filesystem::IsType(const Path&    path,
                        const FileType type)
{
    struct stat st;
    if (stat(path.GetCString(), &st) != 0)
        return false;

    switch (type)
    {
        case kFileType_File:
            return S_ISREG(st.st_mode);

        case kFileType_Directory:
            return S_ISDIR(st.st_mode);

        default:
            return true;

    }
}

bool Filesystem::SetWorkingDirectory(const Path& path)
{
    return chdir(path.GetCString()) == 0;
}

bool Filesystem::GetFullPath(const Path& path,
                             Path&       outFullPath)
{
    char* str = realpath(path.GetCString(), nullptr);
    if (!str)
    {
        return false;
    }

    outFullPath = Path(str, Path::kNormalized);
    free(str);
    return true;
}
