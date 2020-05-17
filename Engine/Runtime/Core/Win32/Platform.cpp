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

#include "Core/Platform.h"

#include "Core/Path.h"

#include "Win32.h"
#include <ShlObj.h>

static LARGE_INTEGER gPerformanceFrequency;

void Platform::Init()
{
    QueryPerformanceFrequency(&gPerformanceFrequency);
}

std::string Platform::GetProgramName()
{
    std::wstring str;
    str.resize(MAX_PATH);
    DWORD ret = GetModuleFileName(nullptr, &str[0], MAX_PATH);
    if (ret == 0)
    {
        Fatal("Failed to get program name: 0x%", GetLastError());
    }

    str.resize(ret);
    return Path(WideToUTF8(str), Path::kUnnormalizedPlatform).GetBaseFileName();
}

Path Platform::GetUserDirectory()
{
    PWSTR str;
    HRESULT ret = SHGetKnownFolderPath(FOLDERID_LocalAppData,
                                       KF_FLAG_DEFAULT,
                                       nullptr,
                                       &str);
    Assert(ret == S_OK);
    Unused(ret);

    Path path(WideToUTF8(str), Path::kUnnormalizedPlatform);
    CoTaskMemFree(str);
    return path;
}

uint64_t Platform::GetPerformanceCounter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    counter.QuadPart *= 1000000000;
    counter.QuadPart /= gPerformanceFrequency.QuadPart;
    return counter.QuadPart;
}