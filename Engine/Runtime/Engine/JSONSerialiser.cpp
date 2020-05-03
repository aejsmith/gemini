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
    JSONScope&                          GetCurrentScope(const char* const name);

    bool                                BeginScope(const char* const     name,
                                                   const JSONScope::Type type);

    rapidjson::Value&                   AddMember(JSONScope&        scope,
                                                  const char* const name,
                                                  rapidjson::Value& value);

    rapidjson::Value*                   GetMember(JSONScope&        scope,
                                                  const char* const name);

public:
    bool                                writing;
    rapidjson::Document                 document;

    /** Map of object addresses to pre-existing IDs (serialising). */
    HashMap<const Object*, uint32_t>    objectToIDMap;

    /** Map of IDs to pre-existing objects (deserialising). */
    HashMap<uint32_t, ObjPtr<>>         idToObjectMap;

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

JSONScope& JSONState::GetCurrentScope(const char* const name)
{
    JSONScope& scope = this->scopes.back();

    if (name)
    {
        Assert(scope.type != JSONScope::kArray);
    }
    else
    {
        Assert(scope.type == JSONScope::kArray);
    }

    return scope;
}

bool JSONState::BeginScope(const char* const     name,
                           const JSONScope::Type type)
{
    rapidjson::Type jsonType = (type == JSONScope::kArray)
                                   ? rapidjson::kArrayType
                                   : rapidjson::kObjectType;

    JSONScope& scope = GetCurrentScope(name);

    rapidjson::Value* value;

    if (this->writing)
    {
        rapidjson::Value initValue(jsonType);
        value = &AddMember(scope, name, initValue);
    }
    else
    {
        value = GetMember(scope, name);
        if (!value || value->GetType() != jsonType)
        {
            return false;
        }
    }

    this->scopes.emplace_back(type, *value);
    return true;
}

rapidjson::Value& JSONState::AddMember(JSONScope&        scope,
                                       const char* const name,
                                       rapidjson::Value& value)
{
    if (scope.type == JSONScope::kArray)
    {
        scope.value.PushBack(value,
                             this->document.GetAllocator());

        rapidjson::Value& ret = scope.value[scope.value.Size() - 1];
        return ret;
    }
    else
    {
        Assert(!scope.value.HasMember(name));

        scope.value.AddMember(rapidjson::StringRef(name),
                              value,
                              this->document.GetAllocator());

        rapidjson::Value &ret = scope.value[name];
        return ret;
    }
}

rapidjson::Value* JSONState::GetMember(JSONScope&        scope,
                                       const char* const name)
{
    if (scope.type == JSONScope::kArray)
    {
        if (scope.nextIndex >= scope.value.Size())
        {
            return nullptr;
        }

        return &scope.value[scope.nextIndex++];
    }
    else
    {
        if (!scope.value.HasMember(name))
        {
            return nullptr;
        }

        return &scope.value[name];
    }
}

JSONSerialiser::JSONSerialiser() :
    mState (nullptr)
{
}

ByteArray JSONSerialiser::Serialise(const Object* const object)
{
    JSONState state;

    mState          = &state;
    mState->writing = true;
    mState->document.SetArray();

    /* Serialise the object. */
    AddObject(object);

    /* Write out the JSON stream. */
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    mState->document.Accept(writer);

    ByteArray data(buffer.GetSize());
    memcpy(data.Get(), buffer.GetString(), buffer.GetSize());

    mState = nullptr;
    return data;
}

uint32_t JSONSerialiser::AddObject(const Object* const object)
{
    /* Create a new object. */
    const uint32_t id = mState->document.Size();
    mState->document.PushBack(rapidjson::kObjectType,
                              mState->document.GetAllocator());

    rapidjson::Value& value = mState->document[mState->document.Size() - 1];

    /* Record it in the object map so we don't serialise it again. */
    mState->objectToIDMap.insert(std::make_pair(object, id));

    /* Write out the type of the object, as well as its ID. The ID is not used
     * in deserialisation (it's done based on order of appearance in the array),
     * but we write it anyway because JSON is meant to be a human readable
     * format, and having the ID helps to understand it. */
    value.AddMember("objectClass",
                    rapidjson::StringRef(object->GetMetaClass().GetName()),
                    mState->document.GetAllocator());
    value.AddMember("objectID",
                    id,
                    mState->document.GetAllocator());

    /* Serialise the object in a new scope. */
    mState->scopes.emplace_back(JSONScope::kObject, value);
    SerialiseObject(object);
    mState->scopes.pop_back();

    return id;
}

ObjPtr<> JSONSerialiser::Deserialise(const ByteArray& data,
                                     const MetaClass& expectedClass)
{
    JSONState state;

    mState          = &state;
    mState->writing = false;

    /* Parse the JSON stream. */
    mState->document.Parse(reinterpret_cast<const char*>(data.Get()), data.GetSize());

    if (mState->document.HasParseError())
    {
        LogError("Parse error in serialised data (at %zu): %s",
                 mState->document.GetErrorOffset(),
                 rapidjson::GetParseError_En(mState->document.GetParseError()));

        return nullptr;
    }

    /* The object to return is the first object in the file. */
    ObjPtr<> object = FindObject(0, expectedClass);

    mState = nullptr;
    return object;
}

ObjPtr<> JSONSerialiser::FindObject(const uint32_t   id,
                                    const MetaClass& metaClass)
{
    /* Check if it is already deserialised. */
    auto existing = mState->idToObjectMap.find(id);
    if (existing != mState->idToObjectMap.end())
    {
        return existing->second;
    }

    if (!mState->document.IsArray())
    {
        LogError("Serialised data is not an array");
        return nullptr;
    }
    else if (id >= mState->document.Size())
    {
        LogError("Invalid serialised object ID %zu (only %zu objects available)",
                 id,
                 mState->document.Size());

        return nullptr;
    }

    rapidjson::Value& value = mState->document[id];

    if (!value.HasMember("objectClass"))
    {
        LogError("Serialised object %zu does not have an 'objectClass' value", id);
        return nullptr;
    }

    /* The serialised object or any objects it refers to may contain references
     * back to itself. Therefore, to ensure that we don't deserialise the object
     * multiple times, we must store it in our map before we call its
     * Deserialise() method. Serialiser::DeserialiseObject() sets the object
     * pointer referred to as soon it has constructed the object, before calling
     * Deserialise(). This ensures that deserialised references to the object
     * will point to the correct object. */
    auto inserted = mState->idToObjectMap.insert(std::make_pair(id, nullptr));
    Assert(inserted.second);

    mState->scopes.emplace_back(JSONScope::kObject, value);

    const bool success = DeserialiseObject(value["objectClass"].GetString(),
                                           metaClass,
                                           id == 0,
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

bool JSONSerialiser::BeginGroup(const char* const name)
{
    Assert(mState);

    return mState->BeginScope(name, JSONScope::kGroup);
}

void JSONSerialiser::EndGroup()
{
    Assert(mState);
    Assert(mState->scopes.back().type == JSONScope::kGroup);

    mState->scopes.pop_back();
}

bool JSONSerialiser::BeginArray(const char* const name)
{
    Assert(mState);

    return mState->BeginScope(name, JSONScope::kArray);
}

void JSONSerialiser::EndArray()
{
    Assert(mState);
    Assert(mState->scopes.back().type == JSONScope::kArray);

    mState->scopes.pop_back();
}

void JSONSerialiser::Write(const char* const name,
                           const MetaType&   type,
                           const void* const value)
{
    Assert(mState);
    Assert(mState->writing);

    if (type.IsPointer() && type.GetPointeeType().IsObject())
    {
        /* Object references require special handling. We serialise these as a
         * JSON object containing details of where to find the object. If the
         * reference is null, the JSON object is empty. If the reference refers
         * to a managed asset, it contains an "asset" member containing the
         * asset path. Otherwise, we serialise the object if it has not already
         * been added to the file, and store an "objectID" member referring to
         * it. */
        BeginGroup(name);

        const Object* const object = *reinterpret_cast<const Object* const*>(value);
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

    JSONScope& scope = mState->GetCurrentScope(name);
    auto& allocator  = mState->document.GetAllocator();

    rapidjson::Value jsonValue;

    /* Determine the value to write. */
    if (&type == &MetaType::Lookup<bool>())
    {
        jsonValue.SetBool(*reinterpret_cast<const bool*>(value));
    }
    else if (&type == &MetaType::Lookup<int8_t>())
    {
        jsonValue.SetInt(*reinterpret_cast<const int8_t*>(value));
    }
    else if (&type == &MetaType::Lookup<uint8_t>())
    {
        jsonValue.SetUint(*reinterpret_cast<const uint8_t*>(value));
    }
    else if (&type == &MetaType::Lookup<int16_t>())
    {
        jsonValue.SetInt(*reinterpret_cast<const int16_t*>(value));
    }
    else if (&type == &MetaType::Lookup<uint16_t>())
    {
        jsonValue.SetUint(*reinterpret_cast<const uint16_t*>(value));
    }
    else if (&type == &MetaType::Lookup<int32_t>())
    {
        jsonValue.SetInt(*reinterpret_cast<const int32_t*>(value));
    }
    else if (&type == &MetaType::Lookup<uint32_t>())
    {
        jsonValue.SetUint(*reinterpret_cast<const uint32_t*>(value));
    }
    else if (&type == &MetaType::Lookup<int64_t>())
    {
        jsonValue.SetInt64(*reinterpret_cast<const int64_t*>(value));
    }
    else if (&type == &MetaType::Lookup<uint64_t>())
    {
        jsonValue.SetUint64(*reinterpret_cast<const uint64_t*>(value));
    }
    else if (&type == &MetaType::Lookup<float>())
    {
        jsonValue.SetFloat(*reinterpret_cast<const float*>(value));
    }
    else if (&type == &MetaType::Lookup<double>())
    {
        jsonValue.SetDouble(*reinterpret_cast<const double*>(value));
    }
    else if (&type == &MetaType::Lookup<std::string>())
    {
        auto str = reinterpret_cast<const std::string*>(value);
        jsonValue.SetString(str->c_str(), allocator);
    }
    else if (&type == &MetaType::Lookup<glm::vec2>())
    {
        auto vec = reinterpret_cast<const glm::vec2*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::vec3>())
    {
        auto vec = reinterpret_cast<const glm::vec3*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::vec4>())
    {
        auto vec = reinterpret_cast<const glm::vec4*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
        jsonValue.PushBack(vec->w, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::ivec2>())
    {
        auto vec = reinterpret_cast<const glm::ivec2*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::ivec3>())
    {
        auto vec = reinterpret_cast<const glm::ivec3*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::ivec4>())
    {
        auto vec = reinterpret_cast<const glm::ivec4*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
        jsonValue.PushBack(vec->w, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::uvec2>())
    {
        auto vec = reinterpret_cast<const glm::uvec2*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::uvec3>())
    {
        auto vec = reinterpret_cast<const glm::uvec3*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::uvec4>())
    {
        auto vec = reinterpret_cast<const glm::uvec4*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(vec->x, allocator);
        jsonValue.PushBack(vec->y, allocator);
        jsonValue.PushBack(vec->z, allocator);
        jsonValue.PushBack(vec->w, allocator);
    }
    else if (&type == &MetaType::Lookup<glm::quat>())
    {
        auto quat = reinterpret_cast<const glm::quat*>(value);
        jsonValue.SetArray();
        jsonValue.PushBack(quat->w, allocator);
        jsonValue.PushBack(quat->x, allocator);
        jsonValue.PushBack(quat->y, allocator);
        jsonValue.PushBack(quat->z, allocator);
    }
    else if (type.IsEnum())
    {
        // FIXME: Potentially handle cases where we don't have enum metadata?
        const MetaType::EnumConstantArray& constants = type.GetEnumConstants();
        for (const MetaType::EnumConstant& constant : constants)
        {
            int64_t intValue;

            switch (type.GetSize())
            {
                case 1: intValue = static_cast<int64_t>(*reinterpret_cast<const int8_t* >(value)); break;
                case 2: intValue = static_cast<int64_t>(*reinterpret_cast<const int16_t*>(value)); break;
                case 4: intValue = static_cast<int64_t>(*reinterpret_cast<const int32_t*>(value)); break;
                case 8: intValue = static_cast<int64_t>(*reinterpret_cast<const int64_t*>(value)); break;

                default:
                    Unreachable();
            }

            if (intValue == constant.second)
            {
                jsonValue.SetString(rapidjson::StringRef(constant.first));
                break;
            }
        }

        Assert(!jsonValue.IsNull());
    }
    else
    {
        Fatal("Type '%s' is unsupported for serialisation", type.GetName());
    }

    mState->AddMember(scope, name, jsonValue);
}

bool JSONSerialiser::Read(const char* const name,
                          const MetaType&   type,
                          void* const       outValue)
{
    Assert(mState);
    Assert(!mState->writing);

    if (type.IsPointer() && type.GetPointeeType().IsObject())
    {
        /* See Write() for details on how we handle object references. */
        if (!BeginGroup(name))
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

        auto& metaClass = static_cast<const MetaClass&>(type.GetPointeeType());

        ObjPtr<> ret;

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
            if (type.IsRefcounted())
            {
                *reinterpret_cast<ObjPtr<>*>(outValue) = std::move(ret);
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

    JSONScope& scope = mState->GetCurrentScope(name);

    rapidjson::Value* member = mState->GetMember(scope, name);
    if (!member)
    {
        return false;
    }

    /* Get a reference to make the [] operator usage nicer below. */
    rapidjson::Value& jsonValue = *member;

    if (&type == &MetaType::Lookup<bool>())
    {
        if (!jsonValue.IsBool())
        {
            return false;
        }

        *reinterpret_cast<bool*>(outValue) = jsonValue.GetBool();
    }
    else if (&type == &MetaType::Lookup<int8_t>())
    {
        if (!jsonValue.IsInt())
        {
            return false;
        }

        *reinterpret_cast<int8_t*>(outValue) = jsonValue.GetInt();
    }
    else if (&type == &MetaType::Lookup<uint8_t>())
    {
        if (!jsonValue.IsUint())
        {
            return false;
        }

        *reinterpret_cast<uint8_t*>(outValue) = jsonValue.GetUint();
    }
    else if (&type == &MetaType::Lookup<int16_t>())
    {
        if (!jsonValue.IsInt())
        {
            return false;
        }

        *reinterpret_cast<int16_t*>(outValue) = jsonValue.GetInt();
    }
    else if (&type == &MetaType::Lookup<uint16_t>())
    {
        if (!jsonValue.IsUint())
        {
            return false;
        }

        *reinterpret_cast<uint16_t*>(outValue) = jsonValue.GetUint();
    }
    else if (&type == &MetaType::Lookup<int32_t>())
    {
        if (!jsonValue.IsInt())
        {
            return false;
        }

        *reinterpret_cast<int32_t*>(outValue) = jsonValue.GetInt();
    }
    else if (&type == &MetaType::Lookup<uint32_t>())
    {
        if (!jsonValue.IsUint())
        {
            return false;
        }

        *reinterpret_cast<uint32_t*>(outValue) = jsonValue.GetUint();
    }
    else if (&type == &MetaType::Lookup<int64_t>())
    {
        if (!jsonValue.IsInt64())
        {
            return false;
        }

        *reinterpret_cast<int64_t*>(outValue) = jsonValue.GetInt64();
    }
    else if (&type == &MetaType::Lookup<uint64_t>())
    {
        if (!jsonValue.IsUint64())
        {
            return false;
        }

        *reinterpret_cast<uint64_t*>(outValue) = jsonValue.GetUint64();
    }
    else if (&type == &MetaType::Lookup<float>())
    {
        if (!jsonValue.IsFloat())
        {
            return false;
        }

        *reinterpret_cast<float*>(outValue) = jsonValue.GetFloat();
    }
    else if (&type == &MetaType::Lookup<double>())
    {
        if (!jsonValue.IsDouble())
        {
            return false;
        }

        *reinterpret_cast<double*>(outValue) = jsonValue.GetDouble();
    }
    else if (&type == &MetaType::Lookup<std::string>())
    {
        if (!jsonValue.IsString())
        {
            return false;
        }

        *reinterpret_cast<std::string*>(outValue) = jsonValue.GetString();
    }
    else if (&type == &MetaType::Lookup<glm::vec2>())
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
    else if (&type == &MetaType::Lookup<glm::vec3>())
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
    else if (&type == &MetaType::Lookup<glm::vec4>())
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
    else if (&type == &MetaType::Lookup<glm::ivec2>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 2 ||
            !jsonValue[0u].IsInt() ||
            !jsonValue[1u].IsInt())
        {
            return false;
        }

        *reinterpret_cast<glm::ivec2*>(outValue) = glm::ivec2(jsonValue[0u].GetInt(),
                                                              jsonValue[1u].GetInt());
    }
    else if (&type == &MetaType::Lookup<glm::ivec3>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 3 ||
            !jsonValue[0u].IsInt() ||
            !jsonValue[1u].IsInt() ||
            !jsonValue[2u].IsInt())
        {
            return false;
        }

        *reinterpret_cast<glm::ivec3*>(outValue) = glm::ivec3(jsonValue[0u].GetInt(),
                                                              jsonValue[1u].GetInt(),
                                                              jsonValue[2u].GetInt());
    }
    else if (&type == &MetaType::Lookup<glm::ivec4>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 4 ||
            !jsonValue[0u].IsInt() ||
            !jsonValue[1u].IsInt() ||
            !jsonValue[2u].IsInt() ||
            !jsonValue[3u].IsInt())
        {
            return false;
        }

        *reinterpret_cast<glm::ivec4*>(outValue) = glm::ivec4(jsonValue[0u].GetInt(),
                                                              jsonValue[1u].GetInt(),
                                                              jsonValue[2u].GetInt(),
                                                              jsonValue[3u].GetInt());
    }
    else if (&type == &MetaType::Lookup<glm::uvec2>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 2 ||
            !jsonValue[0u].IsUint() ||
            !jsonValue[1u].IsUint())
        {
            return false;
        }

        *reinterpret_cast<glm::uvec2*>(outValue) = glm::uvec2(jsonValue[0u].GetUint(),
                                                              jsonValue[1u].GetUint());
    }
    else if (&type == &MetaType::Lookup<glm::uvec3>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 3 ||
            !jsonValue[0u].IsUint() ||
            !jsonValue[1u].IsUint() ||
            !jsonValue[2u].IsUint())
        {
            return false;
        }

        *reinterpret_cast<glm::uvec3*>(outValue) = glm::uvec3(jsonValue[0u].GetUint(),
                                                              jsonValue[1u].GetUint(),
                                                              jsonValue[2u].GetUint());
    }
    else if (&type == &MetaType::Lookup<glm::uvec4>())
    {
        if (!jsonValue.IsArray() ||
            jsonValue.Size() != 4 ||
            !jsonValue[0u].IsUint() ||
            !jsonValue[1u].IsUint() ||
            !jsonValue[2u].IsUint() ||
            !jsonValue[3u].IsUint())
        {
            return false;
        }

        *reinterpret_cast<glm::uvec4*>(outValue) = glm::uvec4(jsonValue[0u].GetUint(),
                                                              jsonValue[1u].GetUint(),
                                                              jsonValue[2u].GetUint(),
                                                              jsonValue[3u].GetUint());
    }
    else if (&type == &MetaType::Lookup<glm::quat>())
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
    else if (type.IsEnum())
    {
        if (!jsonValue.IsString())
        {
            return false;
        }

        /* Match the string against a value. */
        const MetaType::EnumConstantArray& constants = type.GetEnumConstants();

        auto constant = constants.begin();
        while (constant != constants.end())
        {
            if (std::strcmp(jsonValue.GetString(), constant->first) == 0)
            {
                switch (type.GetSize())
                {
                    case 1: *reinterpret_cast<int8_t* >(outValue) = static_cast<int8_t >(constant->second); break;
                    case 2: *reinterpret_cast<int16_t*>(outValue) = static_cast<int16_t>(constant->second); break;
                    case 4: *reinterpret_cast<int32_t*>(outValue) = static_cast<int32_t>(constant->second); break;
                    case 8: *reinterpret_cast<int64_t*>(outValue) = static_cast<int64_t>(constant->second); break;

                    default:
                        Unreachable();
                }

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
        Fatal("Type '%s' is unsupported for deserialisation", type.GetName());
    }

    return true;
}


void JSONSerialiser::WriteBinary(const char* const name,
                                 const void* const data,
                                 const size_t      length)
{
    /* Binary data is encoded as base64. We store it in an object containing a
     * "base64" string member, rather than just a plain string, to get better
     * differentiation of type between this and regular strings, as well as to
     * allow the possibility of supporting different encoding schemes later. */
    std::string base64 = Base64::Encode(data, length);

    BeginGroup(name);
    Serialiser::Write("base64", base64);
    EndGroup();
}

bool JSONSerialiser::ReadBinary(const char* const name,
                                ByteArray&        outData)
{
    bool result = false;

    if (BeginGroup(name))
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
