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

#include "Engine/JSONSerialiser.h"

#include "Core/Base64.h"

#include "Engine/AssetManager.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <list>

class JSONScope
{
public:
    enum Type
    {
        kObject,
        kGroup,
        kArray,
    };

public:
                                        JSONScope(const Type        inType,
                                                  rapidjson::Value& inValue);

public:
    Type                                type;
    rapidjson::Value&                   value;
    size_t                              nextIndex;

};

class JSONState
{
public:
    JSONScope&                          GetCurrentScope(const char* const inName);

    bool                                BeginScope(const char* const     inName,
                                                   const JSONScope::Type inType);

    rapidjson::Value&                   AddMember(JSONScope&        inScope,
                                                  const char* const inName,
                                                  rapidjson::Value& inValue);

    rapidjson::Value*                   GetMember(JSONScope&        inScope,
                                                  const char* const inName);

public:
    bool                                writing;
    rapidjson::Document                 document;

    /** Map of object addresses to pre-existing IDs (serialising). */
    HashMap<const Object*, uint32_t>    objectToIDMap;

    /** Map of IDs to pre-existing objects (deserialising). */
    HashMap<uint32_t, ObjectPtr<>>      idToObjectMap;

    /**
     * This is used to keep track of which value we are currently reading from
     * or writing to. Each (Add|Find)Object(), BeginChild() and BeginArray()
     * call pushes a new scope on to the end. Read() and Write() operate on the
     * scope at the end of the list.
     */
    std::list<JSONScope>                scopes;

};

JSONScope::JSONScope(const Type        inType,
                     rapidjson::Value& inValue) :
    type      (inType),
    value     (inValue),
    nextIndex (0)
{
}

JSONScope& JSONState::GetCurrentScope(const char* const inName)
{
    JSONScope& scope = this->scopes.back();

    if (inName)
    {
        Assert(scope.type != JSONScope::kArray);
    }
    else
    {
        Assert(scope.type == JSONScope::kArray);
    }

    return scope;
}

bool JSONState::BeginScope(const char* const     inName,
                           const JSONScope::Type inType)
{
    rapidjson::Type jsonType = (inType == JSONScope::kArray)
                                   ? rapidjson::kArrayType
                                   : rapidjson::kObjectType;

    JSONScope& scope = GetCurrentScope(inName);

    rapidjson::Value* value;

    if (this->writing)
    {
        rapidjson::Value initValue(jsonType);
        value = &AddMember(scope, inName, initValue);
    }
    else
    {
        value = GetMember(scope, inName);
        if (!value || value->GetType() != jsonType)
        {
            return false;
        }
    }

    this->scopes.emplace_back(inType, *value);
    return true;
}

rapidjson::Value& JSONState::AddMember(JSONScope&        inScope,
                                       const char* const inName,
                                       rapidjson::Value& inValue)
{
    if (inScope.type == JSONScope::kArray)
    {
        inScope.value.PushBack(inValue,
                               this->document.GetAllocator());

        rapidjson::Value& ret = inScope.value[inScope.value.Size() - 1];
        return ret;
    }
    else
    {
        Assert(!inScope.value.HasMember(inName));

        inScope.value.AddMember(rapidjson::StringRef(inName),
                                inValue,
                                this->document.GetAllocator());

        rapidjson::Value &ret = inScope.value[inName];
        return ret;
    }
}

rapidjson::Value* JSONState::GetMember(JSONScope&        inScope,
                                       const char* const inName)
{
    if (inScope.type == JSONScope::kArray)
    {
        if (inScope.nextIndex >= inScope.value.Size())
        {
            return nullptr;
        }

        return &inScope.value[inScope.nextIndex++];
    }
    else
    {
        if (!inScope.value.HasMember(inName))
        {
            return nullptr;
        }

        return &inScope.value[inName];
    }
}

JSONSerialiser::JSONSerialiser() :
    mState (nullptr)
{
}

ByteArray JSONSerialiser::Serialise(const Object* const inObject)
{
    JSONState state;

    mState          = &state;
    mState->writing = true;
    mState->document.SetArray();

    /* Serialise the object. */
    AddObject(inObject);

    /* Write out the JSON stream. */
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    mState->document.Accept(writer);

    ByteArray data(buffer.GetSize());
    memcpy(data.Get(), buffer.GetString(), buffer.GetSize());

    mState = nullptr;
    return data;
}

uint32_t JSONSerialiser::AddObject(const Object* const inObject)
{
    /* Create a new object. */
    const uint32_t id = mState->document.Size();
    mState->document.PushBack(rapidjson::kObjectType,
                              mState->document.GetAllocator());

    rapidjson::Value& value = mState->document[mState->document.Size() - 1];

    /* Record it in the object map so we don't serialise it again. */
    mState->objectToIDMap.insert(std::make_pair(inObject, id));

    /* Write out the type of the object, as well as its ID. The ID is not used
     * in deserialisation (it's done based on order of appearance in the array),
     * but we write it anyway because JSON is meant to be a human readable
     * format, and having the ID helps to understand it. */
    value.AddMember("objectClass",
                    rapidjson::StringRef(inObject->GetMetaClass().GetName()),
                    mState->document.GetAllocator());
    value.AddMember("objectID",
                    id,
                    mState->document.GetAllocator());

    /* Serialise the object in a new scope. */
    mState->scopes.emplace_back(JSONScope::kObject, value);
    SerialiseObject(inObject);
    mState->scopes.pop_back();

    return id;
}

ObjectPtr<> JSONSerialiser::Deserialise(const ByteArray& inData,
                                        const MetaClass& inExpectedClass)
{
    JSONState state;

    mState          = &state;
    mState->writing = false;

    /* Parse the JSON stream. */
    mState->document.Parse(reinterpret_cast<const char*>(inData.Get()), inData.GetSize());

    if (mState->document.HasParseError())
    {
        LogError("Parse error in serialised data (at %zu): %s",
                 mState->document.GetErrorOffset(),
                 rapidjson::GetParseError_En(mState->document.GetParseError()));

        return nullptr;
    }

    /* The object to return is the first object in the file. */
    ObjectPtr<> object = FindObject(0, inExpectedClass);

    mState = nullptr;
    return object;
}

ObjectPtr<> JSONSerialiser::FindObject(const uint32_t   inID,
                                       const MetaClass& inMetaClass)
{
    /* Check if it is already deserialised. */
    auto existing = mState->idToObjectMap.find(inID);
    if (existing != mState->idToObjectMap.end())
    {
        return existing->second;
    }

    if (inID >= mState->document.Size())
    {
        LogError("Invalid serialised object ID %zu (only %zu objects available)",
                 inID,
                 mState->document.Size());

        return nullptr;
    }

    rapidjson::Value& value = mState->document[inID];

    if (!value.HasMember("objectClass"))
    {
        LogError("Serialised object %zu does not have an 'objectClass' value", inID);
        return nullptr;
    }

    /* The serialised object or any objects it refers to may contain references
     * back to itself. Therefore, to ensure that we don't deserialise the object
     * multiple times, we must store it in our map before we call its
     * Deserialise() method. Serialiser::DeserialiseObject() sets the object
     * pointer referred to as soon it has constructed the object, before calling
     * Deserialise(). This ensures that deserialised references to the object
     * will point to the correct object. */
    auto inserted = mState->idToObjectMap.insert(std::make_pair(inID, nullptr));
    Assert(inserted.second);

    mState->scopes.emplace_back(JSONScope::kObject, value);

    const bool success = DeserialiseObject(value["objectClass"].GetString(),
                                           inMetaClass,
                                           inID == 0,
                                           inserted.first->second);

    mState->scopes.pop_back();

    if (success)
    {
        return inserted.first->second;
    }
    else
    {
        mState->idToObjectMap.erase(inserted.first);
        return nullptr;
    }
}

bool JSONSerialiser::BeginGroup(const char* const inName)
{
    Assert(mState);

    return mState->BeginScope(inName, JSONScope::kGroup);
}

void JSONSerialiser::EndGroup()
{
    Assert(mState);
    Assert(mState->scopes.back().type == JSONScope::kGroup);

    mState->scopes.pop_back();
}

bool JSONSerialiser::BeginArray(const char* const inName)
{
    Assert(mState);

    return mState->BeginScope(inName, JSONScope::kArray);
}

void JSONSerialiser::EndArray()
{
    Assert(mState);
    Assert(mState->scopes.back().type == JSONScope::kArray);

    mState->scopes.pop_back();
}

void JSONSerialiser::Write(const char* const inName,
                           const MetaType&   inType,
                           const void* const inValue)
{
    Assert(mState);
    Assert(mState->writing);

    if (inType.IsPointer() && inType.GetPointeeType().IsObject())
    {
        /* Object references require special handling. We serialise these as a
         * JSON object containing details of where to find the object. If the
         * reference is null, the JSON object is empty. If the reference refers
         * to a managed asset, it contains an "asset" member containing the
         * asset path. Otherwise, we serialise the object if it has not already
         * been added to the file, and store an "objectID" member referring to
         * it. */
        BeginGroup(inName);

        const Object* const object = *reinterpret_cast<const Object* const*>(inValue);
        if (object)
        {
            /* Check if it is already serialised. We check this before handling
             * assets, because if we are serialising an asset and that contains
             * any child objects, we want any references they contain back to
             * the asset itself to point to the object within the serialised
             * file rather than using an asset path reference. */
            auto existing = mState->objectToIDMap.find(object);

            const Asset* asset;

            if (existing != mState->objectToIDMap.end())
            {
                Serialiser::Write("objectID", existing->second);
            }
            else if ((asset = object_cast<const Asset*>(object)) && asset->IsManaged())
            {
                Serialiser::Write("asset", asset->GetPath());
            }
            else
            {
                const uint32_t id = AddObject(object);
                Serialiser::Write("objectID", id);
            }
        }

        EndGroup();
        return;
    }

    JSONScope& scope = mState->GetCurrentScope(inName);
    auto& allocator  = mState->document.GetAllocator();

    rapidjson::Value jsonValue;

    /* Determine the value to write. */
    if (&inType == &MetaType::Lookup<bool>())
    {
        jsonValue.SetBool(*reinterpret_cast<const bool*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<int8_t>())
    {
        jsonValue.SetInt(*reinterpret_cast<const int8_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<uint8_t>())
    {
        jsonValue.SetUint(*reinterpret_cast<const uint8_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<int16_t>())
    {
        jsonValue.SetInt(*reinterpret_cast<const int16_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<uint16_t>())
    {
        jsonValue.SetUint(*reinterpret_cast<const uint16_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<int32_t>())
    {
        jsonValue.SetInt(*reinterpret_cast<const int32_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<uint32_t>())
    {
        jsonValue.SetUint(*reinterpret_cast<const uint32_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<int64_t>())
    {
        jsonValue.SetInt64(*reinterpret_cast<const int64_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<uint64_t>())
    {
        jsonValue.SetUint64(*reinterpret_cast<const uint64_t*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<float>())
    {
        jsonValue.SetFloat(*reinterpret_cast<const float*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<double>())
    {
        jsonValue.SetDouble(*reinterpret_cast<const double*>(inValue));
    }
    else if (&inType == &MetaType::Lookup<std::string>())
    {
        auto str = reinterpret_cast<const std::string*>(inValue);
        jsonValue.SetString(str->c_str(), allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::vec2>())
    {
        auto vec = reinterpret_cast<const glm::vec2*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::vec3>())
    {
        auto vec = reinterpret_cast<const glm::vec3*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::vec4>())
    {
        auto vec = reinterpret_cast<const glm::vec4*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
        jsonValue.PushBack(vec->w, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::ivec2>())
    {
        auto vec = reinterpret_cast<const glm::ivec2*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::ivec3>())
    {
        auto vec = reinterpret_cast<const glm::ivec3*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::ivec4>())
    {
        auto vec = reinterpret_cast<const glm::ivec4*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
        jsonValue.PushBack(vec->w, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::uvec2>())
    {
        auto vec = reinterpret_cast<const glm::uvec2*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::uvec3>())
    {
        auto vec = reinterpret_cast<const glm::uvec3*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::uvec4>())
    {
        auto vec = reinterpret_cast<const glm::uvec4*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
        jsonValue.PushBack(vec->w, allocator);
    }
    else if (&inType == &MetaType::Lookup<glm::quat>())
    {
        auto quat = reinterpret_cast<const glm::quat*>(inValue);
        jsonValue.SetArray();
        jsonValue.PushBack(quat->w, allocator);
        jsonValue.PushBack(quat->x, allocator);
        jsonValue.PushBack(quat->y, allocator);
        jsonValue.PushBack(quat->z, allocator);
    }
    else if (inType.IsEnum())
    {
        // FIXME: Potentially handle cases where we don't have enum metadata?
        // FIXME: Incorrect where enum size is not an int.
        const MetaType::EnumConstantArray& constants = inType.GetEnumConstants();
        for (const MetaType::EnumConstant& constant : constants)
        {
            if (*reinterpret_cast<const int*>(inValue) == constant.second)
            {
                jsonValue.SetString(rapidjson::StringRef(constant.first));
                break;
            }
        }

        Assert(!jsonValue.IsNull());
    }
    else
    {
        Fatal("Type '%s' is unsupported for serialisation", inType.GetName());
    }

    mState->AddMember(scope, inName, jsonValue);
}

bool JSONSerialiser::Read(const char* const inName,
                          const MetaType&   inType,
                          void* const       outValue)
{
    Assert(mState);
    Assert(!mState->writing);

    if (inType.IsPointer() && inType.GetPointeeType().IsObject())
    {
        /* See Write() for details on how we handle object references. */
        if (!BeginGroup(inName))
        {
            return false;
        }

        /* An empty object indicates a null reference. */
        if (mState->scopes.back().value.MemberCount() == 0)
        {
            *reinterpret_cast<Object **>(outValue) = nullptr;
            EndGroup();
            return true;
        }

        auto& metaClass = static_cast<const MetaClass&>(inType.GetPointeeType());

        ObjectPtr<> ret;

        /* Check if we have an asset path. */
        std::string path;
        if (Serialiser::Read("asset", path))
        {
            ret = AssetManager::Get().Load(path);
            if (ret && !metaClass.IsBaseOf(ret->GetMetaClass()))
            {
                LogError("Class mismatch in serialised data (expected '%s', have '%s')",
                         metaClass.GetName(),
                         ret->GetMetaClass().GetName());

                ret.Reset();
            }
        }
        else
        {
            /* Must be serialised within the file. */
            uint32_t id;
            if (Serialiser::Read("objectID", id))
            {
                ret = FindObject(id, metaClass);
            }
        }

        EndGroup();

        if (ret)
        {
            if (inType.IsRefcounted())
            {
                *reinterpret_cast<ObjectPtr<>*>(outValue) = std::move(ret);
            }
            else
            {
                *reinterpret_cast<Object**>(outValue) = ret;
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    JSONScope& scope = mState->GetCurrentScope(inName);

    rapidjson::Value* member = mState->GetMember(scope, inName);
    if (!member)
    {
        return false;
    }

    /* Get a reference to make the [] operator usage nicer below. */
    rapidjson::Value& jsonValue = *member;

    if (&inType == &MetaType::Lookup<bool>())
    {
        if (!jsonValue.IsBool())
        {
            return false;
        }

        *reinterpret_cast<bool*>(outValue) = jsonValue.GetBool();
    }
    else if (&inType == &MetaType::Lookup<int8_t>())
    {
        if (!jsonValue.IsInt())
        {
            return false;
        }

        *reinterpret_cast<int8_t*>(outValue) = jsonValue.GetInt();
    }
    else if (&inType == &MetaType::Lookup<uint8_t>())
    {
        if (!jsonValue.IsUint())
        {
            return false;
        }

        *reinterpret_cast<uint8_t*>(outValue) = jsonValue.GetUint();
    }
    else if (&inType == &MetaType::Lookup<int16_t>())
    {
        if (!jsonValue.IsInt())
        {
            return false;
        }

        *reinterpret_cast<int16_t*>(outValue) = jsonValue.GetInt();
    }
    else if (&inType == &MetaType::Lookup<uint16_t>())
    {
        if (!jsonValue.IsUint())
        {
            return false;
        }

        *reinterpret_cast<uint16_t*>(outValue) = jsonValue.GetUint();
    }
    else if (&inType == &MetaType::Lookup<int32_t>())
    {
        if (!jsonValue.IsInt())
        {
            return false;
        }

        *reinterpret_cast<int32_t*>(outValue) = jsonValue.GetInt();
    }
    else if (&inType == &MetaType::Lookup<uint32_t>())
    {
        if (!jsonValue.IsUint())
        {
            return false;
        }

        *reinterpret_cast<uint32_t*>(outValue) = jsonValue.GetUint();
    }
    else if (&inType == &MetaType::Lookup<int64_t>())
    {
        if (!jsonValue.IsInt64())
        {
            return false;
        }

        *reinterpret_cast<int64_t*>(outValue) = jsonValue.GetInt64();
    }
    else if (&inType == &MetaType::Lookup<uint64_t>())
    {
        if (!jsonValue.IsUint64())
        {
            return false;
        }

        *reinterpret_cast<uint64_t*>(outValue) = jsonValue.GetUint64();
    }
    else if (&inType == &MetaType::Lookup<float>())
    {
        if (!jsonValue.IsFloat())
        {
            return false;
        }

        *reinterpret_cast<float*>(outValue) = jsonValue.GetFloat();
    }
    else if (&inType == &MetaType::Lookup<double>())
    {
        if (!jsonValue.IsDouble())
        {
            return false;
        }

        *reinterpret_cast<double*>(outValue) = jsonValue.GetDouble();
    }
    else if (&inType == &MetaType::Lookup<std::string>())
    {
        if (!jsonValue.IsString())
        {
            return false;
        }

        *reinterpret_cast<std::string*>(outValue) = jsonValue.GetString();
    }
    else if (&inType == &MetaType::Lookup<glm::vec2>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 2 ||
            !jsonValue[0u].IsNumber() ||
            !jsonValue[1u].IsNumber())
        {
            return false;
        }

        *reinterpret_cast<glm::vec2*>(outValue) = glm::vec2(jsonValue[0u].GetFloat(),
                                                            jsonValue[1u].GetFloat());
    }
    else if (&inType == &MetaType::Lookup<glm::vec3>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 3 ||
            !jsonValue[0u].IsNumber() ||
            !jsonValue[1u].IsNumber() ||
            !jsonValue[2u].IsNumber())
        {
            return false;
        }

        *reinterpret_cast<glm::vec3*>(outValue) = glm::vec3(jsonValue[0u].GetFloat(),
                                                            jsonValue[1u].GetFloat(),
                                                            jsonValue[2u].GetFloat());
    }
    else if (&inType == &MetaType::Lookup<glm::vec4>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 4 ||
            !jsonValue[0u].IsNumber() ||
            !jsonValue[1u].IsNumber() ||
            !jsonValue[2u].IsNumber() ||
            !jsonValue[3u].IsNumber())
        {
            return false;
        }

        *reinterpret_cast<glm::vec4*>(outValue) = glm::vec4(jsonValue[0u].GetFloat(),
                                                            jsonValue[1u].GetFloat(),
                                                            jsonValue[2u].GetFloat(),
                                                            jsonValue[3u].GetFloat());
    }
    else if (&inType == &MetaType::Lookup<glm::quat>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 4 ||
            !jsonValue[0u].IsNumber() ||
            !jsonValue[1u].IsNumber() ||
            !jsonValue[2u].IsNumber() ||
            !jsonValue[3u].IsNumber())
        {
            return false;
        }

        *reinterpret_cast<glm::quat*>(outValue) = glm::quat(jsonValue[0u].GetFloat(),
                                                            jsonValue[1u].GetFloat(),
                                                            jsonValue[2u].GetFloat(),
                                                            jsonValue[3u].GetFloat());
    }
    else if (inType.IsEnum())
    {
        if (!jsonValue.IsString())
        {
            return false;
        }

        /* Match the string against a value. */
        const MetaType::EnumConstantArray& constants = inType.GetEnumConstants();

        auto constant = constants.begin();
        while (constant != constants.end())
        {
            if (std::strcmp(jsonValue.GetString(), constant->first) == 0)
            {
                // FIXME: enum size
                *reinterpret_cast<int*>(outValue) = constant->second;
                break;
            }

            ++constant;
        }

        if (constant == constants.end())
        {
            return false;
        }
    }
    else
    {
        Fatal("Type '%s' is unsupported for deserialisation", inType.GetName());
    }

    return true;
}


void JSONSerialiser::WriteBinary(const char* const inName,
                                 const void* const inData,
                                 const size_t      inLength)
{
    /* Binary data is encoded as base64. We store it in an object containing a
     * "base64" string member, rather than just a plain string, to get better
     * differentiation of type between this and regular strings, as well as to
     * allow the possibility of supporting different encoding schemes later. */
    std::string base64 = Base64::Encode(inData, inLength);

    BeginGroup(inName);
    Serialiser::Write("base64", base64);
    EndGroup();
}

bool JSONSerialiser::ReadBinary(const char* const inName,
                                ByteArray&        outData)
{
    bool result = false;

    if (BeginGroup(inName))
    {
        std::string base64;
        if (Serialiser::Read("base64", base64))
        {
            result = Base64::Decode(base64, outData);
        }

        EndGroup();
    }

    return result;
}
