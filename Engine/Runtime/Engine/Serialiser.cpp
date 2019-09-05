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

void Serialiser::Write(const char* const inName,
                       const bool&       inValue)
{
    Write(inName, MetaType::Lookup<bool>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const int8_t&     inValue)
{
    Write(inName, MetaType::Lookup<int8_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const uint8_t&    inValue)
{
    Write(inName, MetaType::Lookup<uint8_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const int16_t&    inValue)
{
    Write(inName, MetaType::Lookup<int16_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const uint16_t&   inValue)
{
    Write(inName, MetaType::Lookup<uint16_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const int32_t&    inValue)
{
    Write(inName, MetaType::Lookup<int32_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const uint32_t&   inValue)
{
    Write(inName, MetaType::Lookup<uint32_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const int64_t&    inValue)
{
    Write(inName, MetaType::Lookup<int64_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const uint64_t&   inValue)
{
    Write(inName, MetaType::Lookup<uint64_t>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const float&      inValue)
{
    Write(inName, MetaType::Lookup<float>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const double&     inValue)
{
    Write(inName, MetaType::Lookup<double>(), &inValue);
}

void Serialiser::Write(const char* const  inName,
                       const std::string& inValue)
{
    Write(inName, MetaType::Lookup<std::string>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::vec2&  inValue)
{
    Write(inName, MetaType::Lookup<glm::vec2>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::vec3&  inValue)
{
    Write(inName, MetaType::Lookup<glm::vec3>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::vec4&  inValue)
{
    Write(inName, MetaType::Lookup<glm::vec4>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::ivec2& inValue)
{
    Write(inName, MetaType::Lookup<glm::ivec2>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::ivec3& inValue)
{
    Write(inName, MetaType::Lookup<glm::ivec3>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::ivec4& inValue)
{
    Write(inName, MetaType::Lookup<glm::ivec4>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::uvec2& inValue)
{
    Write(inName, MetaType::Lookup<glm::uvec2>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::uvec3& inValue)
{
    Write(inName, MetaType::Lookup<glm::uvec3>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::uvec4& inValue)
{
    Write(inName, MetaType::Lookup<glm::uvec4>(), &inValue);
}

void Serialiser::Write(const char* const inName,
                       const glm::quat&  inValue)
{
    Write(inName, MetaType::Lookup<glm::quat>(), &inValue);
}

bool Serialiser::Read(const char* const inName,
                      bool&             outValue)
{
    return Read(inName, MetaType::Lookup<bool>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      int8_t&           outValue)
{
    return Read(inName, MetaType::Lookup<int8_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      uint8_t&          outValue)
{
    return Read(inName, MetaType::Lookup<uint8_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      int16_t&          outValue)
{
    return Read(inName, MetaType::Lookup<int16_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      uint16_t&         outValue)
{
    return Read(inName, MetaType::Lookup<uint16_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      int32_t&          outValue)
{
    return Read(inName, MetaType::Lookup<int32_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      uint32_t&         outValue)
{
    return Read(inName, MetaType::Lookup<uint32_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      int64_t&          outValue)
{
    return Read(inName, MetaType::Lookup<int64_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      uint64_t&         outValue)
{
    return Read(inName, MetaType::Lookup<uint64_t>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      float&            outValue)
{
    return Read(inName, MetaType::Lookup<float>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      double&           outValue)
{
    return Read(inName, MetaType::Lookup<double>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      std::string&      outValue)
{
    return Read(inName, MetaType::Lookup<std::string>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::vec2&        outValue)
{
    return Read(inName, MetaType::Lookup<glm::vec2>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::vec3&        outValue)
{
    return Read(inName, MetaType::Lookup<glm::vec3>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::vec4&        outValue)
{
    return Read(inName, MetaType::Lookup<glm::vec4>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::ivec2&       outValue)
{
    return Read(inName, MetaType::Lookup<glm::ivec2>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::ivec3&       outValue)
{
    return Read(inName, MetaType::Lookup<glm::ivec3>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::ivec4&       outValue)
{
    return Read(inName, MetaType::Lookup<glm::ivec4>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::uvec2&       outValue)
{
    return Read(inName, MetaType::Lookup<glm::uvec2>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::uvec3&       outValue)
{
    return Read(inName, MetaType::Lookup<glm::uvec3>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::uvec4&       outValue)
{
    return Read(inName, MetaType::Lookup<glm::uvec4>(), &outValue);
}

bool Serialiser::Read(const char* const inName,
                      glm::quat&        outValue)
{
    return Read(inName, MetaType::Lookup<glm::quat>(), &outValue);
}
