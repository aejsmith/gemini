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

MetaType::MetaType(const char* const name,
                   const size_t      size,
                   const uint32_t    traits,
                   const MetaType*   parent) :
    mName          (name),
    mSize          (size),
    mTraits        (traits),
    mParent        (parent),
    mEnumConstants (nullptr)
{
}

const MetaType* MetaType::Allocate(const char* const signature,
                                   const size_t      size,
                                   const uint32_t    traits,
                                   const MetaType*   parent)
{
    /* Derive the type name from the function signature (see LookupImpl). */
    #if defined(__GNUC__)
        std::string name(signature);

        const size_t start = name.rfind("LookupT = ") + 10;
        const size_t len   = name.rfind(", LookupEnable") - start;

        name = name.substr(start, len);
    #elif defined(_MSC_VER)
        std::string name(signature);

        const size_t start = name.rfind("LookupImpl<") + 11;
        const size_t len   = name.rfind(",void") - start;

        name = name.substr(start, len);
    #else
        #error "Unsupported compiler"
    #endif

    return new MetaType(strdup(name.c_str()),
                        size,
                        traits,
                        parent);
}

const char* MetaType::GetEnumConstantName(const int value) const
{
    Assert(IsEnum());

    for (const EnumConstant& constant : *mEnumConstants)
    {
        if (value == constant.second)
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

MetaClass::MetaClass(const char* const         name,
                     const size_t              size,
                     const uint32_t            traits,
                     const MetaClass* const    parent,
                     const ConstructorFunction constructor,
                     const PropertyArray&      properties) :
    MetaType     (name,
                  size,
                  traits | MetaType::kIsObject,
                  parent),
    mConstructor (constructor),
    mProperties  (properties)
{
    auto classRet = GetMetaClassMap().insert(std::make_pair(mName, this));
    Unused(classRet);

    AssertMsg(classRet.second,
              "Registering meta-class '%s' that already exists",
              mName);

    /* Add properties to a map for fast lookup. */
    for (const MetaProperty& property : mProperties)
    {
        auto propRet = mPropertyMap.insert(std::make_pair(property.GetName(), &property));
        Unused(propRet);

        AssertMsg(propRet.second,
                  "Meta-class '%s' has duplicate property '%s'",
                  mName,
                  property.GetName());
    }
}

MetaClass::~MetaClass()
{
    GetMetaClassMap().erase(mName);
}

bool MetaClass::IsBaseOf(const MetaClass& other) const
{
    const MetaClass* current = &other;

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

const MetaProperty* MetaClass::LookupProperty(const char* const name) const
{
    const MetaClass* current = this;

    while (current)
    {
        auto ret = current->mPropertyMap.find(name);
        if (ret != current->mPropertyMap.end())
        {
            return ret->second;
        }

        current = current->GetParent();
    }

    return nullptr;
}

std::vector<const MetaClass*> MetaClass::GetConstructableClasses(const bool sorted) const
{
    std::vector<const MetaClass*> classList;

    MetaClass::Visit(
        [&] (const MetaClass& otherClass)
        {
            if (IsBaseOf(otherClass) && otherClass.IsConstructable())
            {
                classList.emplace_back(&otherClass);
            }
        });

    if (sorted)
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

const MetaClass* MetaClass::Lookup(const std::string& name)
{
    auto& map = GetMetaClassMap();

    auto ret = map.find(name);
    return (ret != map.end()) ? ret->second : nullptr;
}

void MetaClass::Visit(const std::function<void (const MetaClass&)>& function)
{
    for (auto it : GetMetaClassMap())
    {
        function(*it.second);
    }
}

MetaProperty::MetaProperty(const char* const name,
                           const MetaType&   type,
                           const uint32_t    flags,
                           const GetFunction getFunction,
                           const SetFunction setFunction) :
    mName        (name),
    mType        (type),
    mFlags       (flags),
    mGetFunction (getFunction),
    mSetFunction (setFunction)
{
}

/** Look up a property and check that it is the given type. */
static const MetaProperty* LookupAndCheckProperty(const MetaClass&  metaClass,
                                                  const char* const name,
                                                  const MetaType&   type)
{
    const MetaProperty* property = metaClass.LookupProperty(name);
    if (!property)
    {
        LogError("Attempt to access non-existant property '%s' on class '%s'",
                 name,
                 metaClass.GetName());

        return nullptr;
    }

    if (&type != &property->GetType())
    {
        LogError("Type mismatch accessing property '%s' on class '%s', requested '%s', actual '%s'",
                 name,
                 metaClass.GetName(),
                 type.GetName(),
                 property->GetType().GetName());

        return nullptr;
    }

    return property;
}

bool Object::GetProperty(const char* const name,
                         const MetaType&   type,
                         void* const       outValue) const
{
    const MetaProperty* property = LookupAndCheckProperty(GetMetaClass(),
                                                          name,
                                                          type);
    if (!property)
    {
        return false;
    }

    property->GetValue(this, outValue);
    return true;
}

bool Object::SetProperty(const char* const name,
                         const MetaType&   type,
                         const void* const value)
{
    const MetaProperty *property = LookupAndCheckProperty(GetMetaClass(),
                                                          name,
                                                          type);
    if (!property)
    {
        return false;
    }

    property->SetValue(this, value);
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

void Object::Serialise(Serialiser& serialiser) const
{
    /* Serialise properties into a separate group. */
    serialiser.BeginGroup("objectProperties");

    /* We should serialise base class properties first. It may be that, for
     * example, the set method of a derived class property depends on the value
     * of a base class property. */
    std::function<void (const MetaClass*)> SerialiseProperties =
        [&] (const MetaClass* metaClass)
        {
            if (metaClass->GetParent())
            {
                SerialiseProperties(metaClass->GetParent());
            }

            for (const MetaProperty& property : metaClass->GetProperties())
            {
                if (property.IsTransient())
                {
                    continue;
                }

                SerialisationBuffer buffer(property.GetType());
                property.GetValue(this, buffer.data);

                serialiser.Write(property.GetName(),
                                 property.GetType(),
                                 buffer.data);
            }
        };

    SerialiseProperties(&GetMetaClass());

    serialiser.EndGroup();
}

void Object::Deserialise(Serialiser &serialiser)
{
    if (serialiser.BeginGroup("objectProperties"))
    {
        std::function<void (const MetaClass*)> DeserialiseProperties =
            [&] (const MetaClass* metaClass)
            {
                if (metaClass->GetParent())
                {
                    DeserialiseProperties(metaClass->GetParent());
                }

                for (const MetaProperty& property : metaClass->GetProperties())
                {
                    if (property.IsTransient())
                    {
                        continue;
                    }

                    SerialisationBuffer buffer(property.GetType());

                    const bool result = serialiser.Read(property.GetName(),
                                                        property.GetType(),
                                                        buffer.data);
                    if (result)
                    {
                        property.SetValue(this, buffer.data);
                    }
                }
            };

        DeserialiseProperties(&GetMetaClass());

        serialiser.EndGroup();
    }
}

ObjPtr<> Object::LoadObject(const Path&      path,
                            const MetaClass& expectedClass)
{
    UPtr<File> file(Filesystem::OpenFile(path));
    if (!file)
    {
        LogError("Failed to open '%s'", path.GetCString());
        return nullptr;
    }

    ByteArray serialisedData(file->GetSize());
    if (!file->Read(serialisedData.Get(), file->GetSize()))
    {
        LogError("Failed to read '%s'", path.GetCString());
        return nullptr;
    }

    /* TODO: Assumed as JSON for now. When we have binary serialisation this
     * will need to detect the file type. */
    JSONSerialiser serialiser;
    ObjPtr<> object = serialiser.Deserialise(serialisedData, expectedClass);
    if (!object)
    {
        LogError("Failed to deserialise '%s'", path.GetCString());
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

void Object::CustomDebugUIEditor(const uint32_t        flags,
                                 std::vector<Object*>& ioChildren)
{
}

template <typename T, typename Func>
static void DebugUIPropertyEditor(Object* const       object,
                                  const MetaProperty& property,
                                  Func                function)
{
    if (&property.GetType() != &MetaType::Lookup<T>())
    {
        return;
    }

    ImGui::PushID(&property);

    ImGui::Text(property.GetName());

    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);

    T value;
    object->GetProperty<T>(property.GetName(), value);

    if (function(&value))
    {
        object->SetProperty<T>(property.GetName(), value);
    }

    ImGui::PopItemWidth();
    ImGui::NextColumn();

    ImGui::PopID();
}

static bool EnumItemGetter(void* const  data,
                           const int    index,
                           const char** outText)
{
    const auto& constants = *reinterpret_cast<const MetaType::EnumConstantArray*>(data);

    if (outText)
    {
        *outText = constants[index].first;
    }

    return true;
}

static void DebugUIEnumPropertyEditor(Object* const       object,
                                      const MetaProperty& property)
{
    if (!property.GetType().IsEnum())
    {
        return;
    }

    ImGui::PushID(&property);

    ImGui::Text(property.GetName());
    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);

    const MetaType& type                         = property.GetType();
    const MetaType::EnumConstantArray& constants = type.GetEnumConstants();

    /* Get current value. */
    int64_t value;
    switch (type.GetSize())
    {
        case 1:
        {
            int8_t tmp;
            object->GetProperty(property.GetName(), property.GetType(), &tmp);
            value = static_cast<int64_t>(tmp);
            break;
        }

        case 2:
        {
            int16_t tmp;
            object->GetProperty(property.GetName(), property.GetType(), &tmp);
            value = static_cast<int64_t>(tmp);
            break;
        }

        case 4:
        {
            int32_t tmp;
            object->GetProperty(property.GetName(), property.GetType(), &tmp);
            value = static_cast<int64_t>(tmp);
            break;
        }

        case 8:
        {
            object->GetProperty(property.GetName(), property.GetType(), &value);
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
                object->SetProperty(property.GetName(), property.GetType(), &tmp);
                break;
            }

            case 2:
            {
                const int16_t tmp = static_cast<int16_t>(value);
                object->SetProperty(property.GetName(), property.GetType(), &tmp);
                break;
            }

            case 4:
            {
                const int32_t tmp = static_cast<int32_t>(value);
                object->SetProperty(property.GetName(), property.GetType(), &tmp);
                break;
            }

            case 8:
            {
                object->SetProperty(property.GetName(), property.GetType(), &value);
                break;
            }
        }
    }

    ImGui::PushItemWidth(-1);
    ImGui::NextColumn();
    ImGui::PopID();
}

static void DebugUIAssetPropertyEditor(Object* const       object,
                                       const MetaProperty& property)
{
    if (!property.GetType().IsPointer() || !property.GetType().GetPointeeType().IsObject())
    {
        return;
    }

    const MetaClass& pointeeClass = static_cast<const MetaClass&>(property.GetType().GetPointeeType());

    if (!Asset::staticMetaClass.IsBaseOf(pointeeClass))
    {
        return;
    }

    ImGui::PushID(&property);

    ImGui::Text(property.GetName());
    ImGui::NextColumn();

    AssetPtr asset;
    object->GetProperty(property.GetName(), property.GetType(), &asset);

    const bool activate = ImGui::Button("Select");

    ImGui::SameLine();
    ImGui::Text("%s", (asset) ? asset->GetPath().c_str() : "null");

    if (AssetManager::Get().DebugUIAssetSelector(asset, pointeeClass, activate))
    {
        object->SetProperty(property.GetName(), property.GetType(), &asset);
    }

    ImGui::NextColumn();

    ImGui::PopID();
}

static void DebugUIObjectPropertyEditor(Object* const         object,
                                        const MetaProperty&   property,
                                        const uint32_t        flags,
                                        std::vector<Object*>& ioChildren)
{
    if (!property.GetType().IsPointer() || !property.GetType().GetPointeeType().IsObject())
    {
        return;
    }

    const MetaClass& pointeeClass = static_cast<const MetaClass&>(property.GetType().GetPointeeType());

    /* TODO: Need an editor for Entity/Component references. */
    if (Asset::staticMetaClass.IsBaseOf(pointeeClass) ||
        Entity::staticMetaClass.IsBaseOf(pointeeClass) ||
        Component::staticMetaClass.IsBaseOf(pointeeClass))
    {
        return;
    }

    ImGui::PushID(&property);

    ImGui::Text(property.GetName());
    ImGui::NextColumn();

    ObjPtr<> child;
    object->GetProperty(property.GetName(), property.GetType(), &child);

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
        object->SetProperty(property.GetName(), property.GetType(), &child);
    }

    if (flags & Object::kDebugUIEditor_IncludeChildren && child)
    {
        ioChildren.emplace_back(child);
    }

    ImGui::NextColumn();

    ImGui::PopID();
}

static void DebugUIPropertyEditors(Object* const         object,
                                   const MetaClass&      metaClass,
                                   const uint32_t        flags,
                                   std::vector<Object*>& ioChildren)
{
    /* Display base class properties first. */
    if (metaClass.GetParent())
    {
        DebugUIPropertyEditors(object, *metaClass.GetParent(), flags, ioChildren);
    }

    for (const MetaProperty& property : metaClass.GetProperties())
    {
        /* These all do nothing if the type does not match. */

        ImGui::AlignTextToFramePadding();

        DebugUIPropertyEditor<bool>(
            object,
            property,
            [&] (bool* ioValue)
            {
                return ImGui::Checkbox("", ioValue);
            });

        #define INT_EDITOR(type, imType) \
            DebugUIPropertyEditor<type>( \
                object, \
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
            object,
            property,
            [&] (float* ioValue)
            {
                return ImGui::InputFloat("", ioValue, 0.0f, 0.0f, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::vec2>(
            object,
            property,
            [&] (glm::vec2* ioValue)
            {
                return ImGui::InputFloat2("", &ioValue->x, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::vec3>(
            object,
            property,
            [&] (glm::vec3* ioValue)
            {
                return ImGui::InputFloat3("", &ioValue->x, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::vec4>(
            object,
            property,
            [&] (glm::vec4* ioValue)
            {
                return ImGui::InputFloat4("", &ioValue->x, -1, ImGuiInputTextFlags_EnterReturnsTrue);
            });

        DebugUIPropertyEditor<glm::quat>(
            object,
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
            object,
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

        DebugUIEnumPropertyEditor(object, property);
        DebugUIAssetPropertyEditor(object, property);
        DebugUIObjectPropertyEditor(object, property, flags, ioChildren);
    }
}

void Object::DebugUIEditor(const uint32_t flags,
                           bool*          outDestroyObject)
{
    bool open = true;

    if (outDestroyObject)
    {
        Assert(flags & kDebugUIEditor_AllowDestruction);
        *outDestroyObject = false;
    }
    else
    {
        Assert(!(flags & kDebugUIEditor_AllowDestruction));
    }

    if (!ImGui::CollapsingHeader(GetMetaClass().GetName(),
                                 (flags & kDebugUIEditor_AllowDestruction) ? &open : nullptr,
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
    DebugUIPropertyEditors(this, GetMetaClass(), flags, children);

    /* Custom editors for things that cannot be handled by the property system. */
    CustomDebugUIEditor(flags, children);

    ImGui::Columns(1);

    if (!children.empty() && flags & kDebugUIEditor_IncludeChildren)
    {
        for (Object* child : children)
        {
            ImGui::Indent();
            ImGui::BeginChild(child->GetMetaClass().GetName());
            child->DebugUIEditor(flags & ~kDebugUIEditor_AllowDestruction);
            ImGui::EndChild();
        }
    }

    ImGui::PopID();
}
