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

#include "Core/CoreDefs.h"

#include "Core/Path.h"

#if !GEMINI_PLATFORM_WIN32
    #include <sys/ioctl.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <ctime>

void FatalLogImpl(const char* const file,
                  const int         line,
                  const char* const format,
                  ...)
{
    va_list args;

    va_start(args, format);
    std::string message = std::string("Fatal Error: ") + StringUtils::VFormat(format, args);
    va_end(args);

    LogImpl(kLogLevel_Error,
            file,
            line,
            "%s",
            message.c_str());
}

void FatalImpl()
{
    abort();
}

void LogVImpl(const LogLevel    level,
              const char* const file,
              const int         line,
              const char* const format,
              va_list           args)
{
    std::string message = StringUtils::VFormat(format, args);

    time_t t = time(nullptr);
    struct tm local;
    #if GEMINI_PLATFORM_WIN32
        localtime_s(&local, &t);
    #else
        localtime_r(&t, &local);
    #endif

    char timeString[20];
    strftime(timeString,
             sizeof(timeString),
             "%Y-%m-%d %H:%M:%S",
             &local);

    std::string fileDetails;
    if (file)
    {
        Path path(file, Path::kUnnormalizedPlatform);
        fileDetails = StringUtils::Format("%s:%d",
                                          path.GetFileName().GetCString(),
                                          line);
    }

    FILE* stream = (level < kLogLevel_Error) ? stdout : stderr;
    const char* levelString = "";

    #if GEMINI_PLATFORM_WIN32
        switch (level)
        {
            case kLogLevel_Debug:
                levelString = "[DEBUG  ]";
                break;
            case kLogLevel_Info:
                levelString = "[INFO   ]";
                break;
            case kLogLevel_Warning:
                levelString = "[WARNING]";
                break;
            case kLogLevel_Error:
                levelString = "[ERROR  ]";
                break;
        }

        fprintf(stream,
                "%s %s %s  (%s)\n",
                timeString,
                levelString,
                message.c_str(),
                fileDetails.c_str());
    #else
        switch (level)
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

        fprintf(stream,
                "%s%s \033[0m%s",
                levelString,
                timeString,
                message.c_str());

        if (file)
        {
            struct winsize w;
            ioctl(0, TIOCGWINSZ, &w);

            fprintf(stream,
                    "\033[0;34m%*s\033[0m\n",
                    w.ws_col - std::strlen(timeString) - message.length() - 2,
                    fileDetails.c_str());
        }
        else
        {
            fprintf(stream, "\n");
        }
    #endif
}

void LogImpl(const LogLevel    level,
             const char* const file,
             const int         line,
             const char* const format,
             ...)
{
    va_list args;

    va_start(args, format);
    LogVImpl(level, file, line, format, args);
    va_end(args);
}
