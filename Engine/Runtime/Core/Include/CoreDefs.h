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

#pragma once

#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <utility>

#if defined(__GNUC__)
    #define FORCEINLINE             __attribute__((always_inline))
    #define NOINLINE                __attribute__((noinline))
    #define Likely(x)               __builtin_expect(!!(x), 1)
    #define Unlikely(x)             __builtin_expect(!!(x), 0)
    #define CompilerUnreachable()   __builtin_unreachable()
#else
    #error "Compiler is not supported"
#endif

#if INTPTR_MAX != INT64_MAX
    #error "Non-64-bit platforms are not supported"
#endif

[[noreturn]] extern void FatalImpl(const char* const inFile,
                                   const int         inLine,
                                   const char* const inFormat,
                                   ...);

/**
 * This function should be called to indicate that an unrecoverable error has
 * occurred at runtime. It results in an immediate shut down of the engine and
 * displays an error message to the user in release builds, and calls abort()
 * on debug builds to allow the error to be caught in a debugger. This function
 * does not return.
 */
#define Fatal(inFormat, ...) \
    FatalImpl(__FILE__, __LINE__, inFormat, ##__VA_ARGS__)

/**
 * Check that a condition is true. If it is not, the engine will abort with an
 * error message giving the condition that failed.
 */
#define Assert(inCondition) \
    do \
    { \
        if (Unlikely(!(inCondition))) \
            FatalImpl(__FILE__, __LINE__, "Assertion failed: %s", #inCondition); \
    } \
    while (0)

/**
 * Check that a condition is true. If it is not, the engine will abort with the
 * specified error message.
 */

#define AssertMsg(inCondition, inFormat, ...) \
    do \
    { \
        if (Unlikely(!(inCondition))) \
            FatalImpl(__FILE__, __LINE__, inFormat, ##__VA_ARGS__); \
    } \
    while (0)

/**
 * This hints to the compiler that the point where it is used is unreachable.
 * If it is reached then a fatal error will be raised in debug, otherwise in
 * release it will be undefined behaviour.
 */
#if ORION_BUILD_DEBUG
    #define Unreachable() \
        Fatal("Unreachable() statement was reached");
    #define UnreachableMsg(...) \
        Fatal(__VA_ARGS__);
#else
    #define Unreachable() \
        CompilerUnreachable();
    #define UnreachableMsg() \
        CompilerUnreachable();
#endif

enum LogLevel
{
    kLogLevel_Debug,
    kLogLevel_Info,
    kLogLevel_Warning,
    kLogLevel_Error,
};

extern void LogVImpl(const LogLevel    inLevel,
                     const char* const inFile,
                     const int         inLine,
                     const char* const inFormat,
                     va_list           inArgs);

extern void LogImpl(const LogLevel    inLevel,
                    const char* const inFile,
                    const int         inLine,
                    const char* const inFormat,
                    ...);

#define LogDebug(inFormat, ...) \
    LogImpl(kLogLevel_Debug, __FILE__, __LINE__, inFormat, ##__VA_ARGS__)

#define LogInfo(inFormat, ...) \
    LogImpl(kLogLevel_Info, __FILE__, __LINE__, inFormat, ##__VA_ARGS__)

#define LogWarning(inFormat, ...) \
    LogImpl(kLogLevel_Warning, __FILE__, __LINE__, inFormat, ##__VA_ARGS__)

#define LogError(inFormat, ...) \
    LogImpl(kLogLevel_Error, __FILE__, __LINE__, inFormat, ##__VA_ARGS__)
