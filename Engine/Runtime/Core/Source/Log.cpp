/*
 * Copyright (C) 2018 Alex Smith
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

#include "CoreDefs.h"

#include "Path.h"

#include <sys/ioctl.h>

#include <cstdio>
#include <cstdlib>

void FatalImpl(const char* const inFile,
               const int         inLine,
               const char* const inFormat,
               ...)
{
    va_list args;

    va_start(args, inFormat);
    std::string message = std::string("Fatal Error: ") + StringUtils::VFormat(inFormat, args);
    va_end(args);

    LogImpl(kLogLevel_Error,
            inFile,
            inLine,
            "%s",
            message.c_str());

    abort();
}

void LogVImpl(const LogLevel    inLevel,
              const char* const inFile,
              const int         inLine,
              const char* const inFormat,
              va_list           inArgs)
{
    std::string message = StringUtils::VFormat(inFormat, inArgs);

    time_t t = time(nullptr);
    struct tm local;
    localtime_r(&t, &local);

    char timeString[20];
    strftime(timeString,
             sizeof(timeString),
            "%Y-%m-%d %H:%M:%S",
            &local);

    Path path(inFile, Path::kUnnormalizedPlatform);
    std::string fileDetails = StringUtils::Format("%s:%d",
                                                  path.GetFileName().GetCString(),
                                                  inLine);

    const char* levelString = "";

    switch (inLevel)
    {
        case kLogLevel_Debug:
            levelString = "\033[1;30m";
            break;

        case kLogLevel_Info:
            levelString = "\033[1;34m";
            break;

        case kLogLevel_Warning:
            levelString = "\033[1;33m";
            break;

        case kLogLevel_Error:
            levelString = "\033[1;31m";
            break;

    }

    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    fprintf((inLevel < kLogLevel_Error) ? stdout : stderr,
            "%s%s \033[0m%s\033[0;34m%*s\033[0m\n",
            levelString,
            timeString,
            message.c_str(),
            w.ws_col - std::strlen(timeString) - message.length() - 2,
            fileDetails.c_str());
}

void LogImpl(const LogLevel    inLevel,
             const char* const inFile,
             const int         inLine,
             const char* const inFormat,
             ...)
{
    va_list args;

    va_start(args, inFormat);
    LogVImpl(inLevel, inFile, inLine, inFormat, args);
    va_end(args);
}
