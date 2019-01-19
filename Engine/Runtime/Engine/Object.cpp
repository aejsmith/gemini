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

/**
 * Notes:
 *  - Currently we do not globally track registered names for all MetaTypes
 *    like we do for MetaClasses. This is for two reasons: firstly, because
 *    MetaTypes are registered dynamically a given type may not be registered
 *    at the time it is looked up, and secondly because I can't think of a need
 *    to be able to look up a non-Object type by name.
 *
 * TODO:
 *  - Can we enforce at compile time that properties must be a supported type,
 *    to ensure we don't run into issues with serialisation (SerialisationBuffer
 *    for example needs to handle the type properly). Perhaps inject something
 *    into the objgen-generated code, similar to HasSerialise, to check it.
 *  - A Variant class could be used instead of SerialisationBuffer?
 */

#include "Engine/Object.h"

#include "Engine/Serialiser.h"

#include <map>
#include <new>

MetaType::MetaType(const char* const inName,
                   const size_t      inSize,
                   const uint32_t    inTraits,
                   const MetaType*   inParent) :
    mName          (inName),
    mSize          (inSize),
    mTraits        (inTraits),
    mParent        (inParent),
    mEnumConstants (nullptr)
{
}

const MetaType* MetaType::Allocate(const char* const inSignature,
                                   const size_t      inSize,
                                   const uint32_t    inTraits,
                                   const MetaType*   inParent)
{
    /* Derive the type name from the function signature (see LookupImpl). */
    #if defined(__GNUC__)
        std::string name(inSignature);

        const size_t start = name.rfind("LookupT = ") + 10;
        const size_t len   = name.rfind(", LookupEnable") - start;

        name = name.substr(start, len);
    #elif defined(_MSC_VER)
        std::string name(inSignature);

        const size_t start = name.rfind("LookupImpl<") + 11;
        const size_t len   = name.rfind(",void") - start;

        name = name.substr(start, len);
    #else
        #error "Unsupported compiler"
    #endif

    return new MetaType(strdup(name.c_str()),
                        inSize,
                        inTraits,
                        inParent);
}

/** Get the global map of all registered MetaClass instances. */
static auto& GetMetaClassMap()
{
    /* Only used from global constructors, no synchronisation is needed. */
    static std::map<std::string, const MetaClass*> map;
    return map;
}

MetaClass::MetaClass(const char* const         inName,
                     const size_t              inSize,
                     const uint32_t            inTraits,
                     const MetaClass* const    inParent,
                     const ConstructorFunction inConstructor,
                     const PropertyArray&      inProperties) :
    MetaType     (inName,
                  inSize,
                  inTraits | MetaType::kIsObject,
                  inParent),
    mConstructor (inConstructor),
    mProperties  (inProperties)
{
    auto ret = GetMetaClassMap().insert(std::make_pair(mName, this));
    Unused(ret);

    AssertMsg(ret.second,
              "Registering meta-class '%s' that already exists",
              mName);

    /* Add properties to a map for fast lookup. */
    for (const MetaProperty& property : mProperties)
    {
        auto ret = mPropertyMap.insert(std::make_pair(property.GetName(), &property));
        Unused(ret);

        AssertMsg(ret.second,
                  "Meta-class '%s' has duplicate property '%s'",
                  mName,
                  property.GetName());
    }
}

MetaClass::~MetaClass()
{
    GetMetaClassMap().erase(mName);
}

bool MetaClass::IsBaseOf(const MetaClass& inOther) const
{
    const MetaClass* current = &inOther;

    while (current)
    {
        if (current == this)
        {
            return true;
        }

        current = current->GetParent();
    }

    return false;
}

ObjectPtr<> MetaClass::Construct() const
{
    AssertMsg(mTraits & kIsPublicConstructable,
              "Attempt to construct object of class '%s' which is not publically constructable",
              mName);

    return mConstructor();
}

ObjectPtr<> MetaClass::ConstructPrivate() const
{
    AssertMsg(mTraits & kIsConstructable,
              "Attempt to construct object of class '%s' which is not constructable",
              mName);

    return mConstructor();
}

const MetaProperty* MetaClass::LookupProperty(const char* const inName) const
{
    const MetaClass* current = this;

    while (current)
    {
        auto ret = current->mPropertyMap.find(inName);
        if (ret != current->mPropertyMap.end())
        {
            return ret->second;
        }

        current = current->GetParent();
    }

    return nullptr;
}

const MetaClass* MetaClass::Lookup(const std::string& inName)
{
    auto& map = GetMetaClassMap();

    auto ret = map.find(inName);
    return (ret != map.end()) ? ret->second : nullptr;
}

void MetaClass::Visit(const std::function<void (const MetaClass&)>& inFunction)
{
    for (auto it : GetMetaClassMap())
    {
        inFunction(*it.second);
    }
}

MetaProperty::MetaProperty(const char* const inName,
                           const MetaType&   inType,
                           const uint32_t    inFlags,
                           const GetFunction inGetFunction,
                           const SetFunction inSetFunction) :
    mName        (inName),
    mType        (inType),
    mFlags       (inFlags),
    mGetFunction (inGetFunction),
    mSetFunction (inSetFunction)
{
}

/** Look up a property and check that it is the given type. */
static const MetaProperty* LookupAndCheckProperty(const MetaClass&  inMetaClass,
                                                  const char* const inName,
                                                  const MetaType&   inType)
{
    const MetaProperty* property = inMetaClass.LookupProperty(inName);
    if (!property)
    {
        LogError("Attempt to access non-existant property '%s' on class '%s'",
                 inName,
                 inMetaClass.GetName());

        return nullptr;
    }

    if (&inType != &property->GetType())
    {
        LogError("Type mismatch accessing property '%s' on class '%s', requested '%s', actual '%s'",
                 inName,
                 inMetaClass.GetName(),
                 inType.GetName(),
                 property->GetType().GetName());

        return nullptr;
    }

    return property;
}

bool Object::GetProperty(const char* const inName,
                         const MetaType&   inType,
                         void* const       outValue) const
{
    const MetaProperty* property = LookupAndCheckProperty(GetMetaClass(),
                                                          inName,
                                                          inType);
    if (!property)
    {
        return false;
    }

    property->GetValue(this, outValue);
    return true;
}

bool Object::SetProperty(const char* const inName,
                         const MetaType&   inType,
                         const void* const inValue)
{
    const MetaProperty *property = LookupAndCheckProperty(GetMetaClass(),
                                                          inName,
                                                          inType);
    if (!property)
    {
        return false;
    }

    property->SetValue(this, inValue);
    return true;
}

/**
 * We need some temporary storage when (de)serialising properties. The trouble
 * is that for non-POD types we must ensure that the constructor/destructor is
 * called on the buffer, as property get functions and Serialiser::Read()
 * assume that the buffer is constructed. This class allocates a buffer and
 * calls the constructor/destructor as necessary. We only need to handle types
 * that are supported as properties here.
 */
struct SerialisationBuffer
{
    const MetaType*     type;
    uint8_t*            data;

    SerialisationBuffer(const MetaType& inType) :
        type (&inType),
        data (new uint8_t[type->GetSize()])
    {
        if (this->type == &MetaType::Lookup<std::string>())
        {
            new (this->data) std::string();
        }
        else if (this->type->IsPointer() && this->type->IsRefcounted())
        {
            new (this->data) ReferencePtr<RefCounted>();
        }
    }

    ~SerialisationBuffer()
    {
        if (this->type == &MetaType::Lookup<std::string>())
        {
            /* Work around clang bug. The standard allows:
             *   reinterpret_cast<std::string*>(data)->~string();
             * but clang does not. */
            using namespace std;
            reinterpret_cast<string*>(this->data)->~string();
        }
        else if (this->type->IsPointer() && this->type->IsRefcounted())
        {
            reinterpret_cast<ReferencePtr<RefCounted>*>(this->data)->~ReferencePtr();
        }
    }
};

void Object::Serialise(Serialiser& inSerialiser) const
{
    /* Serialise properties into a separate group. */
    inSerialiser.BeginGroup("objectProperties");

    /* We should serialise base class properties first. It may be that, for
     * example, the set method of a derived class property depends on the value
     * of a base class property. */
    std::function<void (const MetaClass*)> SerialiseProperties =
        [&] (const MetaClass* inMetaClass)
        {
            if (inMetaClass->GetParent())
            {
                SerialiseProperties(inMetaClass->GetParent());
            }

            for (const MetaProperty& property : inMetaClass->GetProperties())
            {
                if (property.IsTransient())
                {
                    continue;
                }

                SerialisationBuffer buffer(property.GetType());
                property.GetValue(this, buffer.data);

                inSerialiser.Write(property.GetName(),
                                   property.GetType(),
                                   buffer.data);
            }
        };

    SerialiseProperties(&GetMetaClass());

    inSerialiser.EndGroup();
}

void Object::Deserialise(Serialiser &inSerialiser)
{
    if (inSerialiser.BeginGroup("objectProperties"))
    {
        std::function<void (const MetaClass*)> DeserialiseProperties =
            [&] (const MetaClass* inMetaClass)
            {
                if (inMetaClass->GetParent())
                {
                    DeserialiseProperties(inMetaClass->GetParent());
                }

                for (const MetaProperty& property : inMetaClass->GetProperties())
                {
                    if (property.IsTransient())
                    {
                        continue;
                    }

                    SerialisationBuffer buffer(property.GetType());

                    const bool result = inSerialiser.Read(property.GetName(),
                                                          property.GetType(),
                                                          buffer.data);
                    if (result)
                    {
                        property.SetValue(this, buffer.data);
                    }
                }
            };

        DeserialiseProperties(&GetMetaClass());

        inSerialiser.EndGroup();
    }
}
