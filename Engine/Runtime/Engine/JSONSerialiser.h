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

#pragma once

#include "Engine/Serialiser.h"

class JSONState;

class JSONSerialiser final : public Serialiser
{
public:
                                JSONSerialiser();

public:
    ByteArray                   Serialise(const Object* const inObject) override;

    ObjectPtr<>                 Deserialise(const ByteArray& inData,
                                            const MetaClass& inExpectedClass) override;

    using Serialiser::Deserialise;

    bool                        BeginGroup(const char* const inName) override;
    void                        EndGroup() override;

    bool                        BeginArray(const char* const inName) override;
    void                        EndArray() override;

protected:
    void                        Write(const char* const inName,
                                      const MetaType&   inType,
                                      const void* const inValue) override;

    bool                        Read(const char* const inName,
                                     const MetaType&   inType,
                                     void* const       outValue) override;

private:
    uint32_t                    AddObject(const Object* const inObject);

    ObjectPtr<>                 FindObject(const uint32_t   inID,
                                           const MetaClass& inMetaClass);

private:
    /* Internal state. Stored in a separate structure to avoid pulling
     * RapidJSON into the public header. */
    JSONState*                  mState;

};
