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

#include "Core/Filesystem.h"

#include "Engine/AssetManager.h"
#include "Engine/DebugWindow.h"
#include "Engine/JSONSerialiser.h"
#include "Engine/Serialiser.h"

#include "Entity/Entity.h"
#include "Entity/Component.h"

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

const char* MetaType::GetEnumConstantName(const int inValue) const
{
    Assert(IsEnum());

    for (const EnumConstant& constant : *mEnumConstants)
    {
        if (inValue == constant.second)
        {
            return constant.first;
        }
    }

    return nullptr;
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

ObjPtr<> MetaClass::Construct() const
{
    AssertMsg(mTraits & kIsPublicConstructable,
              "Attempt to construct object of class '%s' which is not publically constructable",
              mName);

    return mConstructor();
}

ObjPtr<> MetaClass::ConstructPrivate() const
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

std::vector<const MetaClass*> MetaClass::GetConstructableClasses(const bool inSorted) const
{
    std::vector<const MetaClass*> classList;

    MetaClass::Visit(
        [&] (const MetaClass& inOtherClass)
        {
            if (IsBaseOf(inOtherClass) && inOtherClass.IsConstructable())
            {
                classList.emplace_back(&inOtherClass);
            }
        });

    if (inSorted)
    {
        std::sort(
            classList.begin(),
            classList.end(),
            [] (const MetaClass* a, const MetaClass* b)
            {
                return strcmp(a->GetName(), b->GetName()) < 0;
            });
    }

    return classList;
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
            new (this->data) RefPtr<RefCounted>();
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
            reinterpret_cast<RefPtr<RefCounted>*>(this->data)->~RefPtr();
        }

        delete[] this->data;
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

ObjPtr<> Object::LoadObject(const Path&      inPath,
                            const MetaClass& inExpectedClass)
{
    UPtr<File> file(Filesystem::OpenFile(inPath));
    if (!file)
    {
        LogError("Failed to open '%s'", inPath.GetCString());
        return nullptr;
    }

    ByteArray serialisedData(file->GetSize());
    if (!file->Read(serialisedData.Get(), file->GetSize()))
    {
        LogError("Failed to read '%s'", inPath.GetCString());
        return nullptr;
    }

    /* TODO: Assumed as JSON for now. When we have binary serialisation this
     * will need to detect the file type. */
    JSONSerialiser serialiser;
    ObjPtr<> object = serialiser.Deserialise(serialisedData, inExpectedClass);
    if (!object)
    {
        LogError("Failed to deserialise '%s'", inPath.GetCString());
        return nullptr;
    }

    return object;
}

/**
 * Debug UI functions.
 */

const MetaClass* MetaClass::DebugUIClassSelector() const
{
    static ImGuiTextFilter filter;
    ImGui::PushItemWidth(-1);
    filter.Draw("");
    ImGui::PopItemWidth();

    ImGui::BeginChild("ClassSelector", ImVec2(250, 150), false);

    const auto classes      = GetConstructableClasses(true);
    const MetaClass* result = nullptr;

    for (const MetaClass* metaClass : classes)
    {
        if (filter.PassFilter(metaClass->GetName()))
        {
            if (ImGui::MenuItem(metaClass->GetName()))
            {
                result = metaClass;
                break;
            }
        }
    }

    ImGui::EndChild();
    return result;
}

void Object::CustomDebugUIEditor(const uint32_t        inFlags,
                                 std::vector<Object*>& ioChildren)
{
}

template <typename T, typename Func>
static void DebugUIPropertyEditor(Object* const       inObject,
                                  const MetaProperty& inProperty,
                                  Func                inFunction)
{
    if (&inProperty.GetType() != &MetaType::Lookup<T>())
    {
        return;
    }

    ImGui::PushID(&inProperty);

    ImGui::Text(inProperty.GetName());

    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);

    T value;
    inObject->GetProperty<T>(inProperty.GetName(), value);

    if (inFunction(&value))
    {
        inObject->SetProperty<T>(inProperty.GetName(), value);
    }

    ImGui::PopItemWidth();
    ImGui::NextColumn();

    ImGui::PopID();
}

static bool EnumItemGetter(void* const  inData,
                           const int    inIndex,
                           const char** outText)
{
    const auto& constants = *reinterpret_cast<const MetaType::EnumConstantArray*>(inData);

    if (outText)
    {
        *outText = constants[inIndex].first;
    }

    return true;
}

static void DebugUIEnumPropertyEditor(Object* const       inObject,
                                      const MetaProperty& inProperty)
{
    if (!inProperty.GetType().IsEnum())
    {
        return;
    }

    ImGui::PushID(&inProperty);

    ImGui::Text(inProperty.GetName());
    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);

    const MetaType& type                         = inProperty.GetType();
    const MetaType::EnumConstantArray& constants = type.GetEnumConstants();

    /* Get current value. */
    int64_t value;
    switch (type.GetSize())
    {
        case 1:
        {
            int8_t tmp;
            inObject->GetProperty(inProperty.GetName(), inProperty.GetType(), &tmp);
            value = static_cast<int64_t>(tmp);
            break;
        }

        case 2:
        {
            int16_t tmp;
            inObject->GetProperty(inProperty.GetName(), inProperty.GetType(), &tmp);
            value = static_cast<int64_t>(tmp);
            break;
        }

        case 4:
        {
            int32_t tmp;
            inObject->GetProperty(inProperty.GetName(), inProperty.GetType(), &tmp);
            value = static_cast<int64_t>(tmp);
            break;
        }

        case 8:
        {
            inObject->GetProperty(inProperty.GetName(), inProperty.GetType(), &value);
            break;
        }

        default:
        {
            Unreachable();
        }
    }

    /* Match it against a constant. */
    int index;
    for (index = 0; static_cast<size_t>(index) < constants.size(); index++)
    {
        if (value == constants[index].second)
        {
            break;
        }
    }

    if (ImGui::Combo("",
                     &index,
                     EnumItemGetter,
                     const_cast<MetaType::EnumConstantArray*>(&constants),
                     constants.size()))
    {
        value = constants[index].second;

        switch (type.GetSize())
        {
            case 1:
            {
                const int8_t tmp = static_cast<int8_t>(value);
                inObject->SetProperty(inProperty.GetName(), inProperty.GetType(), &tmp);
                break;
            }

            case 2:
            {
                const int16_t tmp = static_cast<int16_t>(value);
                inObject->SetProperty(inProperty.GetName(), inProperty.GetType(), &tmp);
                break;
            }

            case 4:
            {
                const int32_t tmp = static_cast<int32_t>(value);
                inObject->SetProperty(inProperty.GetName(), inProperty.GetType(), &tmp);
                break;
            }

            case 8:
            {
                inObject->SetProperty(inProperty.GetName(), inProperty.GetType(), &value);
                break;
            }
        }
    }

    ImGui::PushItemWidth(-1);
    ImGui::NextColumn();
    ImGui::PopID();
}

static void DebugUIAssetPropertyEditor(Object* const       inObject,
                                       const MetaProperty& inProperty)
{
    if (!inProperty.GetType().IsPointer() || !inProperty.GetType().GetPointeeType().IsObject())
    {
        return;
    }

    const MetaClass& pointeeClass = static_cast<const MetaClass&>(inProperty.GetType().GetPointeeType());

    if (!Asset::staticMetaClass.IsBaseOf(pointeeClass))
    {
        return;
    }

    ImGui::PushID(&inProperty);

    ImGui::Text(inProperty.GetName());
    ImGui::NextColumn();

    AssetPtr asset;
    inObject->GetProperty(inProperty.GetName(), inProperty.GetType(), &asset);

    const bool activate = ImGui::Button("Select");

    ImGui::SameLine();
    ImGui::Text("%s", (asset) ? asset->GetPath().c_str() : "null");

    if (AssetManager::Get().DebugUIAssetSelector(asset, pointeeClass, activate))
    {
        inObject->SetProperty(inProperty.GetName(), inProperty.GetType(), &asset);
    }

    ImGui::NextColumn();

    ImGui::PopID();
}

static void DebugUIObjectPropertyEditor(Object* const         inObject,
                                        const MetaProperty&   inProperty,
                                        const uint32_t        inFlags,
                                        std::vector<Object*>& ioChildren)
{
    if (!inProperty.GetType().IsPointer() || !inProperty.GetType().GetPointeeType().IsObject())
    {
        return;
    }

    const MetaClass& pointeeClass = static_cast<const MetaClass&>(inProperty.GetType().GetPointeeType());

    /* TODO: Need an editor for Entity/Component references. */
    if (Asset::staticMetaClass.IsBaseOf(pointeeClass) ||
        Entity::staticMetaClass.IsBaseOf(pointeeClass) ||
        Component::staticMetaClass.IsBaseOf(pointeeClass))
    {
        return;
    }

    ImGui::PushID(&inProperty);

    ImGui::Text(inProperty.GetName());
    ImGui::NextColumn();

    ObjPtr<> child;
    inObject->GetProperty(inProperty.GetName(), inProperty.GetType(), &child);

    const bool newSelected   = ImGui::Button("New");
    ImGui::SameLine();
    const bool clearSelected = ImGui::Button("Clear");
    ImGui::SameLine();
    ImGui::Text("%s", (child) ? child->GetMetaClass().GetName() : "null");

    bool set = false;

    if (clearSelected)
    {
        child.Reset();
        set = true;
    }
    else if (newSelected)
    {
        ImGui::OpenPopup("New Object");
    }
    if (ImGui::BeginPopupModal("New Object", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        const MetaClass* const objectClass = pointeeClass.DebugUIClassSelector();
        if (objectClass)
        {
            ImGui::CloseCurrentPopup();

            child = objectClass->Construct();
            set   = true;
        }

        if (ImGui::Button("Cancel", ImVec2(-1, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (set)
    {
        inObject->SetProperty(inProperty.GetName(), inProperty.GetType(), &child);
    }

    if (inFlags & Object::kDebugUIEditor_IncludeChildren && child)
    {
        ioChildren.emplace_back(child);
    }

    ImGui::NextColumn();

    ImGui::PopID();
}

static void DebugUIPropertyEditors(Object* const         inObject,
                                   const MetaClass&      inMetaClass,
                                   const uint32_t        inFlags,
                                   std::vector<Object*>& ioChildren)
{
    /* Display base class properties first. */
    if (inMetaClass.GetParent())
    {
        DebugUIPropertyEditors(inObject, *inMetaClass.GetParent(), inFlags, ioChildren);
    }

    for (const MetaProperty& property : inMetaClass.GetProperties())
    {
        /* These all do nothing if the type does not match. */

        ImGui::AlignTextToFramePadding();

        DebugUIPropertyEditor<bool>(
            inObject,
            property,
            [&] (bool* ioValue)
            {
                return ImGui::Checkbox("", ioValue);
            });

        #define INT_EDITOR(type, imType) \
            DebugUIPropertyEditor<type>( \
                inObject, \
                property, \
                [&] (type* ioValue) \
                { \
                    return ImGui::InputScalar("", imType, ioValue, nullptr, nullptr, nullptr, ImGuiInputTextFlags_EnterReturnsTrue); \
                });

        INT_EDITOR(int8_t,   ImGuiDataType_S8);
        INT_EDITOR(uint8_t,  ImGuiDataType_U8);
        INT_EDITOR(int16_t,  ImGuiDataType_S16);
        INT_EDITOR(uint16_t, ImGuiDataType_U16);
        INT_EDITOR(int32_t,  ImGuiDataType_S32);
        INT_EDITOR(uint32_t, ImGuiDataType_U32);
        INT_EDITOR(int64_t,  ImGuiDataType_S64);
        INT_EDITOR(uint64_t, ImGuiDataType_U64);

        #undef INT_EDITOR

        DebugUIPropertyEditor<float>(
            inObject,
            property,
            [&] (float* ioValue)
            {
                return ImGui::InputFloat("", ioValue, 0.0f, 0.0f, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::vec2>(
            inObject,
            property,
            [&] (glm::vec2* ioValue)
            {
                return ImGui::InputFloat2("", &ioValue->x, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::vec3>(
            inObject,
            property,
            [&] (glm::vec3* ioValue)
            {
                return ImGui::InputFloat3("", &ioValue->x, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::vec4>(
            inObject,
            property,
            [&] (glm::vec4* ioValue)
            {
                return ImGui::InputFloat4("", &ioValue->x, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::quat>(
            inObject,
            property,
            [&] (glm::quat* ioValue)
            {
                glm::vec3 eulerAngles = glm::eulerAngles(*ioValue);

                eulerAngles = glm::vec3(glm::degrees(eulerAngles.x),
                                        glm::degrees(eulerAngles.y),
                                        glm::degrees(eulerAngles.z));

                if (ImGui::SliderFloat3("", &eulerAngles.x, -180.0f, 180.0f))
                {
                    eulerAngles = glm::vec3(glm::radians(eulerAngles.x),
                                            glm::radians(eulerAngles.y),
                                            glm::radians(eulerAngles.z));

                    *ioValue = glm::quat(eulerAngles);
                    return true;
                }
                else
                {
                    return false;
                }
            });

        DebugUIPropertyEditor<std::string>(
            inObject,
            property,
            [&] (std::string* ioValue)
            {
                char str[256];
                strncpy(str, ioValue->c_str(), ArraySize(str) - 1);
                str[ArraySize(str) - 1] = 0;

                if (ImGui::InputText("", str, ArraySize(str), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    *ioValue = str;
                    return true;
                }
                else
                {
                    return false;
                }
            });

        DebugUIEnumPropertyEditor(inObject, property);
        DebugUIAssetPropertyEditor(inObject, property);
        DebugUIObjectPropertyEditor(inObject, property, inFlags, ioChildren);
    }
}

void Object::DebugUIEditor(const uint32_t inFlags,
                           bool*          outDestroyObject)
{
    bool open = true;

    if (outDestroyObject)
    {
        Assert(inFlags & kDebugUIEditor_AllowDestruction);
        *outDestroyObject = false;
    }
    else
    {
        Assert(!(inFlags & kDebugUIEditor_AllowDestruction));
    }

    if (!ImGui::CollapsingHeader(GetMetaClass().GetName(),
                                 (inFlags & kDebugUIEditor_AllowDestruction) ? &open : nullptr,
                                 ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    else if (!open)
    {
        *outDestroyObject = true;
        return;
    }

    ImGui::PushID(this);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, ImGui::GetWindowContentRegionWidth() * 0.3f);

    std::vector<Object*> children;

    /* Generic editors based on class properties. */
    DebugUIPropertyEditors(this, GetMetaClass(), inFlags, children);

    /* Custom editors for things that cannot be handled by the property system. */
    CustomDebugUIEditor(inFlags, children);

    ImGui::Columns(1);

    if (!children.empty() && inFlags & kDebugUIEditor_IncludeChildren)
    {
        for (Object* child : children)
        {
            ImGui::Indent();
            ImGui::BeginChild(child->GetMetaClass().GetName());
            child->DebugUIEditor(inFlags & ~kDebugUIEditor_AllowDestruction);
            ImGui::EndChild();
        }
    }

    ImGui::PopID();
}
