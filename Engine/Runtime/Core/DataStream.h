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

#include "Core/String.h"

enum SeekMode
{
    kSeekMode_Set,              /**< Set the offset to the specified value. */
    kSeekMode_Current,          /**< Set the offset relative to the current offset. */
    kSeekMode_End,              /**< Set the offset relative to the end of the file. */
};

class DataStream
{
public:
    virtual                     ~DataStream() {}

    /**
     * Stream properties.
     */

    virtual uint64_t            GetSize() const = 0;

    /**
     * Stored offset I/O.
     */

    virtual bool                Read(void* const outBuffer, const size_t size) = 0;
    virtual bool                Write(const void* const buffer, const size_t size) = 0;
    virtual bool                Seek(const SeekMode mode, const int64_t offset) = 0;
    virtual uint64_t            GetOffset() const = 0;

    /**
     * Reads from the stream until the next line break into the supplied string.
     * The actual line terminator will not be returned.
     */
    bool                        ReadLine(std::string& outLine);

    /**
     * Specific offset I/O.
     */

    virtual bool                Read(void* const    outBuffer,
                                     const size_t   size,
                                     const uint64_t offset) = 0;

    virtual bool                Write(const void* const buffer,
                                      const size_t      size,
                                      const uint64_t    offset) = 0;

protected:
                                DataStream() {}
                                DataStream(const DataStream&) {}

    DataStream&                 operator =(const DataStream&) { return *this; }

};
