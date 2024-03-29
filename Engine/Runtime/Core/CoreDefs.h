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

#pragma once

#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

/**
 * Compiler-specific macros.
 */

#if defined(__GNUC__)
    #define FORCEINLINE             __attribute__((always_inline))
    #define NOINLINE                __attribute__((noinline))
    #define Likely(x)               __builtin_expect(!!(x), 1)
    #define Unlikely(x)             __builtin_expect(!!(x), 0)
    #define CompilerUnreachable()   __builtin_unreachable()
#elif defined(_MSC_VER)
    #define FORCEINLINE             __forceinline
    #define NOINLINE                __declspec(noinline)
    #define Likely(x)               (x)
    #define Unlikely(x)             (x)
    #define CompilerUnreachable()   abort()
#else
    #error "Compiler is not supported"
#endif

#if INTPTR_MAX != INT64_MAX
    #error "Non-64-bit platforms are not supported"
#endif

/**
 * Helper types.
 */

/** Shorter alias for std::unique_ptr. */
template <typename T>
using UPtr = std::unique_ptr<T>;

/**
 * Helper macros.
 */

/** Break into the debugger. */
#if GEMINI_BUILD_DEBUG
    #if GEMINI_PLATFORM_WIN32
        #define DebugBreak()        __debugbreak()
    #else
        #define DebugBreak()        __asm__ volatile("int3")
    #endif
#else
    #define DebugBreak()
#endif

/** Indicate that a variable may be unused (e.g. only used on debug builds). */
#define Unused(x) ((void)(x))

/**
 * Logging functions/macros.
 */

extern void FatalLogImpl(const char* const file,
                         const int         line,
                         const char* const format,
                         ...);

[[noreturn]] void FatalImpl();

/**
 * This function should be called to indicate that an unrecoverable error has
 * occurred at runtime. It results in an immediate shut down of the engine and
 * displays an error message to the user in release builds, and calls abort()
 * on debug builds to allow the error to be caught in a debugger. This function
 * does not return.
 */
#define Fatal(format, ...) \
    do \
    { \
        FatalLogImpl(__FILE__, __LINE__, format, ##__VA_ARGS__); \
        DebugBreak(); \
        FatalImpl(); \
    } \
    while (0)

/**
 * Check that a condition is true. If it is not, the engine will abort with an
 * error message giving the condition that failed.
 */
#if GEMINI_BUILD_DEBUG
    #define Assert(condition) \
        do \
        { \
            if (Unlikely(!(condition))) \
            { \
                FatalLogImpl(__FILE__, __LINE__, "Assertion failed: %s", #condition); \
                DebugBreak(); \
                FatalImpl(); \
            } \
        } \
        while (0)
#else
    #define Assert(condition)
#endif

/**
 * Check that a condition is true. If it is not, the engine will abort with the
 * specified error message.
 */
#if GEMINI_BUILD_DEBUG
    #define AssertMsg(condition, format, ...) \
        do \
        { \
            if (Unlikely(!(condition))) \
            { \
                FatalLogImpl(__FILE__, __LINE__, format, ##__VA_ARGS__); \
                DebugBreak(); \
                FatalImpl(); \
            } \
        } \
        while (0)
#else
    #define AssertMsg(inCondtion, format, ...)
#endif

/**
 * This hints to the compiler that the point where it is used is unreachable.
 * If it is reached then a fatal error will be raised in debug, otherwise in
 * release it will be undefined behaviour.
 */
#if GEMINI_BUILD_DEBUG
    #define Unreachable() \
        Fatal("Unreachable() statement was reached");
    #define UnreachableMsg(...) \
        Fatal(__VA_ARGS__);
#else
    #define Unreachable() \
        CompilerUnreachable();
    #define UnreachableMsg(...) \
        CompilerUnreachable();
#endif

enum LogLevel
{
    kLogLevel_Debug,
    kLogLevel_Info,
    kLogLevel_Warning,
    kLogLevel_Error,
};

extern void LogVImpl(const LogLevel    level,
                     const char* const file,
                     const int         line,
                     const char* const format,
                     va_list           args);

extern void LogImpl(const LogLevel    level,
                    const char* const file,
                    const int         line,
                    const char* const format,
                    ...);

#define LogDebug(format, ...) \
    LogImpl(kLogLevel_Debug, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogInfo(format, ...) \
    LogImpl(kLogLevel_Info, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogWarning(format, ...) \
    LogImpl(kLogLevel_Warning, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogError(format, ...) \
    LogImpl(kLogLevel_Error, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogMessage(level, format, ...) \
    LogImpl(level, __FILE__, __LINE__, format, ##__VA_ARGS__)

/** Type of a thread identifier. */
using ThreadID = size_t;
