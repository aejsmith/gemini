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

#include "Win32.h"

#include <cwchar>

class Win32File final : public File
{
public:
                            Win32File(HANDLE handle);
                            ~Win32File();

    uint64_t                GetSize() const override;
    bool                    Read(void* const outBuffer, const size_t size) override;
    bool                    Write(const void* const buffer, const size_t size) override;
    bool                    Seek(const SeekMode mode, const int64_t offset) override;
    uint64_t                GetOffset() const override;

    bool                    Read(void* const    outBuffer,
                                 const size_t   size,
                                 const uint64_t offset) override;

    bool                    Write(const void* const buffer,
                                  const size_t      size,
                                  const uint64_t    offset) override;
private:
    HANDLE                  mHandle;

};

class Win32Directory final : public Directory
{
public:
                            Win32Directory(const Path& path);
                            ~Win32Directory();

    void                    Reset() override;
    bool                    Next(Entry& outEntry) override;

private:
    std::wstring            mPath;
    HANDLE                  mFind;

};

Win32File::Win32File(HANDLE handle) :
    mHandle (handle)
{
}

Win32File::~Win32File()
{
    CloseHandle(mHandle);
}

uint64_t Win32File::GetSize() const
{
    LARGE_INTEGER size;
    if (!GetFileSizeEx(mHandle, &size))
    {
        return 0;
    }

    return size.QuadPart;
}

bool Win32File::Read(void* const outBuffer, const size_t size)
{
    Assert(size <= std::numeric_limits<DWORD>::max());

    DWORD bytesRead;
    bool ret = ReadFile(mHandle, outBuffer, size, &bytesRead, nullptr);
    return ret && bytesRead == size;
}

bool Win32File::Write(const void* const buffer, const size_t size)
{
    Assert(size <= std::numeric_limits<DWORD>::max());

    DWORD bytesWritten;
    bool ret = WriteFile(mHandle, buffer, size, &bytesWritten, nullptr);
    return ret && bytesWritten == size;
}

bool Win32File::Seek(const SeekMode mode, const int64_t offset)
{
    DWORD method;

    switch (mode)
    {
        case kSeekMode_Set:
            method = FILE_BEGIN;
            break;
        case kSeekMode_Current:
            method = FILE_CURRENT;
            break;
        case kSeekMode_End:
            method = FILE_END;
            break;
        default:
            return false;
    }

    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    return SetFilePointerEx(mHandle, distance, nullptr, method) == 0;
}

uint64_t Win32File::GetOffset() const
{
    LARGE_INTEGER distance, current;
    distance.QuadPart = 0;
    if (!SetFilePointerEx(mHandle, distance, &current, FILE_CURRENT))
    {
        return 0;
    }

    return current.QuadPart;
}

bool Win32File::Read(void* const    outBuffer,
                     const size_t   size,
                     const uint64_t offset)
{
    Assert(size <= std::numeric_limits<DWORD>::max());

    OVERLAPPED overlapped = {};
    overlapped.Offset     = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

    DWORD bytesRead;
    bool ret = ReadFile(mHandle, outBuffer, size, &bytesRead, &overlapped);
    return ret && bytesRead == size;
}

bool Win32File::Write(const void* const buffer,
                      const size_t      size,
                      const uint64_t    offset)
{
    Assert(size <= std::numeric_limits<DWORD>::max());

    OVERLAPPED overlapped = {};
    overlapped.Offset     = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

    DWORD bytesWritten;
    bool ret = WriteFile(mHandle, buffer, size, &bytesWritten, nullptr);
    return ret && bytesWritten == size;
}

Win32Directory::Win32Directory(const Path& path) :
    mFind (INVALID_HANDLE_VALUE)
{
    /* To match the entire directory contents we need a wildcard. */
    mPath = UTF8ToWide(path.ToPlatform() + "\\*");
}

Win32Directory::~Win32Directory()
{
    Reset();
}

void Win32Directory::Reset()
{
    if (mFind != INVALID_HANDLE_VALUE)
    {
        FindClose(mFind);
        mFind = INVALID_HANDLE_VALUE;
    }
}

bool Win32Directory::Next(Entry& outEntry)
{
    WIN32_FIND_DATA findData;

    while (true)
    {
        if (mFind == INVALID_HANDLE_VALUE)
        {
            mFind = FindFirstFile(mPath.c_str(), &findData);
            if (mFind == INVALID_HANDLE_VALUE)
            {
                return false;
            }
        }
        else
        {
            if (!FindNextFile(mFind, &findData))
            {
                Reset();
                return false;
            }
        }

        if (wcscmp(findData.cFileName, L".") && wcscmp(findData.cFileName, L".."))
        {
            break;
        }
    }

    outEntry.name = WideToUTF8(findData.cFileName);

    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        outEntry.type = kFileType_Directory;
    }
    else
    {
        outEntry.type = kFileType_File;
    }

    return true;
}

File* Filesystem::OpenFile(const Path&    path,
                           const FileMode mode)
{
    std::string winPath = path.ToPlatform();

    DWORD desiredAccess = 0;

    if (mode & kFileMode_Read)
    {
        desiredAccess = GENERIC_READ;
    }

    if (mode & kFileMode_Write)
    {
        desiredAccess = GENERIC_WRITE;
    }

    DWORD creationDisposition = OPEN_EXISTING;

    if (mode & kFileMode_Create)
    {
        Assert(mode & kFileMode_Write);

        if (mode & kFileMode_Truncate)
        {
            creationDisposition = CREATE_ALWAYS;
        }
        else
        {
            creationDisposition = OPEN_ALWAYS;
        }
    }
    else if (mode & kFileMode_Truncate)
    {
        creationDisposition = TRUNCATE_EXISTING;
    }

    HANDLE handle = CreateFile(UTF8ToWide(winPath).c_str(),
                               desiredAccess,
                               0,
                               nullptr,
                               creationDisposition,
                               0,
                               nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        LogError("Failed to open file '%s': 0x%x", winPath.c_str(), GetLastError());
        return nullptr;
    }

    return new Win32File(handle);
}

Directory* Filesystem::OpenDirectory(const Path& path)
{
    if (!IsType(path, kFileType_Directory))
    {
        return nullptr;
    }

    return new Win32Directory(path);
}

bool Filesystem::Exists(const Path& path)
{
    std::wstring winPath = UTF8ToWide(path.ToPlatform());

    DWORD attributes = GetFileAttributes(winPath.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
}

bool Filesystem::IsType(const Path&    path,
                        const FileType type)
{
    std::wstring winPath = UTF8ToWide(path.ToPlatform());

    DWORD attributes = GetFileAttributes(winPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    switch (type)
    {
        case kFileType_File:
            return !(attributes & FILE_ATTRIBUTE_DIRECTORY);

        case kFileType_Directory:
            return attributes & FILE_ATTRIBUTE_DIRECTORY;

        default:
            Unreachable();
    }
}

bool Filesystem::SetWorkingDirectory(const Path& path)
{
    std::wstring winPath = UTF8ToWide(path.ToPlatform());
    return SetCurrentDirectory(winPath.c_str());
}

bool Filesystem::GetFullPath(const Path& path,
                             Path&       outFullPath)
{
    std::unique_ptr<wchar_t[]> str(new wchar_t[4096]);
    std::wstring winPath = UTF8ToWide(path.ToPlatform());

    DWORD ret = GetFullPathName(winPath.c_str(), 4096, str.get(), nullptr);
    if (!ret)
    {
        return false;
    }

    outFullPath = Path(WideToUTF8(str.get()), Path::kUnnormalizedPlatform);
    return true;
}
