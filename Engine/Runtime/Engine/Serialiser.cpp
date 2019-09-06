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

#include "Engine/Serialiser.h"

void Serialiser::SerialiseObject(const Object* const inObject)
{
    /* We are a friend of Object, we can call this. */
    inObject->Serialise(*this);
}

bool Serialiser::DeserialiseObject(const char* const  inClassName,
                                   const MetaClass&   inMetaClass,
                                   const bool         inIsPrimary,
                                   ObjectPtr<>&       outObject)
{
    const MetaClass* const givenMetaClass = MetaClass::Lookup(inClassName);
    if (!givenMetaClass)
    {
        LogError("Serialised data contains unknown class '%s'", inClassName);
        return false;
    }

    if (!inMetaClass.IsBaseOf(*givenMetaClass))
    {
        LogError("Class mismatch in serialised data (expected '%s', have '%s')",
                 inMetaClass.GetName(),
                 inClassName);

        return false;
    }

    /* We allow deserialisation of classes that do not have a public constructor. */
    outObject = givenMetaClass->ConstructPrivate();

    if (inIsPrimary && this->postConstructFunction)
    {
        this->postConstructFunction(outObject);
    }

    outObject->Deserialise(*this);
    return true;
}

#define SERIALISER_READ_WRITE(Type) \
    bool Serialiser::Read(const char* const inName, \
                          Type&             outValue) \
    { \
        return Read(inName, MetaType::Lookup<Type>(), &outValue); \
    } \
    \
    void Serialiser::Write(const char* const inName, \
                           const Type&       inValue) \
    { \
        Write(inName, MetaType::Lookup<Type>(), &inValue); \
    }

SERIALISER_READ_WRITE(bool);
SERIALISER_READ_WRITE(int8_t);
SERIALISER_READ_WRITE(uint8_t);
SERIALISER_READ_WRITE(int16_t);
SERIALISER_READ_WRITE(uint16_t);
SERIALISER_READ_WRITE(int32_t);
SERIALISER_READ_WRITE(uint32_t);
SERIALISER_READ_WRITE(int64_t);
SERIALISER_READ_WRITE(uint64_t);
SERIALISER_READ_WRITE(float);
SERIALISER_READ_WRITE(double);
SERIALISER_READ_WRITE(std::string);
SERIALISER_READ_WRITE(glm::vec2);
SERIALISER_READ_WRITE(glm::vec3);
SERIALISER_READ_WRITE(glm::vec4);
SERIALISER_READ_WRITE(glm::ivec2);
SERIALISER_READ_WRITE(glm::ivec3);
SERIALISER_READ_WRITE(glm::ivec4);
SERIALISER_READ_WRITE(glm::uvec2);
SERIALISER_READ_WRITE(glm::uvec3);
SERIALISER_READ_WRITE(glm::uvec4);
SERIALISER_READ_WRITE(glm::quat);
