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

#include "Core/HashTable.h"
#include "Core/RefCounted.h"
#include "Core/Utility.h"

#include <functional>
#include <type_traits>
#include <vector>

class Object;
class Serialiser;

/**
 * Object-specific wrapper for RefPtr. No functional difference between the
 * two, just to clarify intention and to allow for additional Object-specific
 * behaviour to be added later without having to change any other code.
 */
template <typename T = Object>
using ObjPtr = RefPtr<T>;

/**
 * Annotation macros.
 */

/** Macro to define an annotation attribute for ObjectGen. */
#ifdef GEMINI_OBJGEN
    #define META_ATTRIBUTE(type, ...) \
        __attribute__((annotate("gemini:" type ":" #__VA_ARGS__)))
#else
    #define META_ATTRIBUTE(type, ...)
#endif

/** Helper for CLASS() and base Object definition. */
#define DECLARE_STATIC_METACLASS(...) \
    public: \
        static const META_ATTRIBUTE("class", __VA_ARGS__) MetaClass staticMetaClass; \
    private: \
        static ObjPtr<> ClassConstruct();

/**
 * This macro must be placed on every class which derives from Object (directly
 * or indirectly) in order to add and generate definitions for type metadata.
 * It is expected to be placed in the public section of the class.
 *
 * Any header or source file using this macro must have code generated for it
 * using ObjectGen. Source files should include the ObjectGen output manually
 * after the class definition.
 *
 * The parameters to this macro can specify attributes influencing the class
 * behaviour, with the syntax "attribute1": "string", "attribute2": true, ...
 * The following attributes are supported:
 *
 *  - constructable: Set to false to disallow construction of this class through
 *    the object system. This also disables serialisation for the class.
 *
 * Example:
 *
 *   class Foobar : public Object
 *   {
 *       CLASS();
 *       ...
 *   };
 */
#define CLASS(...) \
        DECLARE_STATIC_METACLASS(__VA_ARGS__); \
    public: \
        virtual const MetaClass& GetMetaClass() const override; \
    private:

/**
 * This macro marks a class member as a property. This means it can be accessed
 * through the dynamic property interface in Object, and information about it
 * can be retrieved at runtime. Properties specified using this macro must be
 * public members. For private members which require getter/setter methods, see
 * VPROPERTY().
 *
 * Example:
 *
 *   class Foo
 *   {
 *       CLASS();
 *   public:
 *       PROPERTY() int bar;
 *   };
 */
#define PROPERTY(...) \
    META_ATTRIBUTE("property", __VA_ARGS__)

/**
 * This macro can be used to declare a "virtual property" on a class. Such
 * properties are not directly associated with a member variable, rather getter
 * and setter methods to modify them. This is useful, for instance, to expose a
 * property as a certain type for editing while using a different internal
 * implementation.
 *
 * The getter and settter methods to use can be specified with the "get" and
 * "set" attributes. If these are not specified, then defaults are used - it is
 * assumed that the getter function is named "Get<PropertyName>" (camel case),
 * and the setter is named "Set<PropertyName>". For instance, for a property
 * named "position", the default getter method is "GetPosition" and the setter
 * is "SetPosition".
 *
 * This functionality is implemented by declaring a static member variable in
 * the class named "vprop_<name>", which is picked up by ObjectGen. These
 * variables should never be used (using them should result in link errors
 * because they have no definition). The variable is still declared when
 * compiling outside of ObjectGen so that errors (e.g. bad types, name
 * conflicts) are still reported by the main compiler.
 *
 * Example:
 *
 *   class Foo
 *   {
 *   public:
 *       VPROPERTY(glm::vec3, position);
 *
 *       const glm::vec3& GetPosition() const;
 *       void             SetPosition(const glm::vec3 &position);
 *       ...
 *   };
 */
#define VPROPERTY(type, name, ...) \
    static META_ATTRIBUTE("property", __VA_ARGS__) type vprop_##name

/**
 * This macro explicitly marks an enum to have code generated for it so that
 * strings for its constants are available at runtime. It is not necessary to
 * do this for an enum which is used as a the type of a class property, such
 * enums automatically have code generated. This macro should be used on enums
 * which are not used for properties but need to be manually (de)serialised.
 *
 * Example:
 *
 *   enum ENUM() Foo
 *   {
 *       kBar,
 *       ...
 *   };
 */
#define ENUM(...) \
    META_ATTRIBUTE("enum", __VA_ARGS__)

/**
 * Metadata classes.
 */

/**
 * This provides basic information about a type. For types outside of the
 * object system, it just provides a means of getting the information required
 * by the object system for dynamic property accesses, serialisation, etc.
 * Metadata is generated dynamically the first time it is required. For Object-
 * derived types, this class forms the base of MetaClass, and for these
 * metadata is generated at build time.
 */
class MetaType
{
public:
    /** Type trait flags. */
    enum : uint32_t
    {
        kIsPointer             = (1 << 0),  /**< Is a pointer. */
        kIsRefcounted          = (1 << 1),  /**< Is a reference-counted pointer. */
        kIsEnum                = (1 << 2),  /**< Is an enumeration. */
        kIsObject              = (1 << 3),  /**< Is an Object-derived class. */
        kIsConstructable       = (1 << 4),  /**< Type is constructable through the Object system. */
        kIsPublicConstructable = (1 << 5),  /**< Type is publically constructable. */
    };

    /** Pair describing an enumeration constant. */
    using EnumConstant            = std::pair<const char*, long long>;
    using EnumConstantArray       = std::vector<EnumConstant>;

public:
    const char*                     GetName() const         { return mName; }
    size_t                          GetSize() const         { return mSize; }

    bool                            IsPointer() const       { return mTraits & kIsPointer; }
    bool                            IsRefcounted() const    { return mTraits & kIsRefcounted; }
    bool                            IsEnum() const          { return mTraits & kIsEnum; }
    bool                            IsObject() const        { return mTraits & kIsObject; }

    const MetaType&                 GetPointeeType() const
                                        { Assert(IsPointer()); return *mParent; }

    /**
     * For an enum type, returns a list of pairs of name and value for each
     * possible value of the enum. This should only be used in situations where
     * it is known that metadata for the type has been generated by ObjectGen.
     * This should be the case when the type is used as the type of a property
     * on a class processed by ObjectGen, or when it is explicitly decorated
     * with ENUM().
     */
    const EnumConstantArray&        GetEnumConstants() const
                                        { Assert(IsEnum()); Assert(mEnumConstants); return *mEnumConstants; }

    /** Get the string name of an enum constant (null for unknown constants). */
    const char*                     GetEnumConstantName(const int inValue) const;

    template <typename T>
    static const MetaType&          Lookup() { return LookupImpl<T>::Get(); }

protected:
                                    MetaType(const char* const     inName,
                                             const size_t          inSize,
                                             const uint32_t        inTraits,
                                             const MetaType* const inParent);

protected:
    const char*                     mName;
    size_t                          mSize;
    uint32_t                        mTraits;

    /**
     * Metadata for parent type. For pointers, this gives the type being pointed
     * to. For Object-derived classes, it gives the parent class. Otherwise, it
     * is null.
     */
    const MetaType*                 mParent;

    /**
     * Pointer to a list of name/value pairs for the enum generated by
     * ObjectGen. This is initially set to null, and set by the constructor of
     * the EnumData instance generated by ObjectGen.
     */
    const EnumConstantArray*        mEnumConstants;

private:
    static const MetaType*          Allocate(const char* const     inSignature,
                                             const size_t          inSize,
                                             const uint32_t        inTraits = 0,
                                             const MetaType* const inParent = nullptr);

    /**
     * Lookup implementation.
     */

    /*
     * The type name string is determined using a compiler-provided macro to
     * get the name of the Get() function, from which we can extract the
     * template parameter type. This is thoroughly evil, I love it! Remember to
     * modify Allocate() when adding new compiler support.
     */
    #if defined(__GNUC__)
        #define LOOKUP_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
    #elif defined(_MSC_VER)
        #define LOOKUP_FUNCTION_SIGNATURE __FUNCSIG__
    #else
        #error "Unsupported compiler"
    #endif

    /** Helper to get the MetaType for a type. */
    template <typename LookupT, typename LookupEnable = void>
    struct LookupImpl
    {
        static NOINLINE const MetaType& Get()
        {
            /*
             * For arbitrary types, we dynamically allocate a MetaType the first
             * time it is requested for that type. Store a pointer to the
             * MetaType as a static local variable. The allocation will take
             * place the first time this function is called for a given type,
             * and all calls after that will return the same. Although a copy
             * of this function will be generated for every translation unit
             * it is used in, they will all be merged into one by the linker.
             * We force this to be not inline so the compiler doesn't duplicate
             * guard variable stuff all over the place.
             */
            static const MetaType* type = Allocate(LOOKUP_FUNCTION_SIGNATURE,
                                                   sizeof(LookupT),
                                                   (std::is_enum<LookupT>::value) ? kIsEnum : 0);
            return *type;
        }
    };

    /** Specialization for pointers. */
    template <typename LookupT>
    struct LookupImpl<LookupT, typename std::enable_if<std::is_pointer<LookupT>::value>::type>
    {
        static NOINLINE const MetaType& Get()
        {
            static const MetaType *type = Allocate(LOOKUP_FUNCTION_SIGNATURE,
                                                   sizeof(LookupT),
                                                   kIsPointer,
                                                   &MetaType::Lookup<typename std::remove_pointer<LookupT>::type>());
            return *type;
        }
    };

    /** Specialization for reference-counted pointers. */
    template <typename PointeeT>
    struct LookupImpl<RefPtr<PointeeT>>
    {
        static NOINLINE const MetaType& Get()
        {
            static const MetaType *type = Allocate(LOOKUP_FUNCTION_SIGNATURE,
                                                   sizeof(RefPtr<PointeeT>),
                                                   kIsPointer | kIsRefcounted,
                                                   &MetaType::Lookup<PointeeT>());
            return *type;
        }
    };

    /** Specialization for reference-counted pointers. */
    template <typename PointeeT>
    struct LookupImpl<const RefPtr<PointeeT>>
    {
        static NOINLINE const MetaType& Get()
        {
            static const MetaType *type = Allocate(LOOKUP_FUNCTION_SIGNATURE,
                                                   sizeof(const RefPtr<PointeeT>),
                                                   kIsPointer | kIsRefcounted,
                                                   &MetaType::Lookup<PointeeT>());
            return *type;
        }
    };

    /** Specialization for Object-derived classes to use the static MetaClass. */
    template <typename LookupT>
    struct LookupImpl<LookupT, typename std::enable_if<std::is_base_of<Object, LookupT>::value>::type>
    {
        static FORCEINLINE const MetaType& Get()
        {
            return LookupT::staticMetaClass;
        }
    };

    #undef LOOKUP_FUNCTION_SIGNATURE

public:
    /** Implementation detail for ObjectGen - do not use directly. */
    template <typename T>
    struct EnumData
    {
        EnumData(std::initializer_list<EnumConstant> inInit) :
            constants (inInit)
        {
            /* This is nasty, however there's no particularly nice way of doing
             * this. Since we don't want to require all enums to have code
             * generated for them, we can't for instance have a specialization
             * of LookupImpl for enums that picks up some ObjectGen-generated
             * metadata. We have to associate any ObjectGen metadata we do have
             * with the dynamically generated MetaTypes somehow. */
            const_cast<MetaType&>(MetaType::Lookup<T>()).mEnumConstants = &constants;
        }

        EnumConstantArray constants;
    };
};

/** Metadata about a property. */
class MetaProperty
{
public:
    /** Property behaviour flags. */
    enum : uint32_t
    {
        kTransient = (1 << 0),  /**< Transient, will not be serialised. */
    };

    /** Type of the getter/setter functions defined by ObjectGen. */
    using GetFunction             = void (*)(const Object*, void*);
    using SetFunction             = void (*)(Object*, const void*);

public:
                                    MetaProperty(const char* const inName,
                                                 const MetaType&   inType,
                                                 const uint32_t    inFlags,
                                                 const GetFunction inGetFunction,
                                                 const SetFunction inSetFunction);

public:
    const char*                     GetName() const     { return mName; }
    const MetaType&                 GetType() const     { return mType; }

    bool                            IsTransient() const { return mFlags & kTransient; }

private:
    void                            GetValue(const Object* const inObject,
                                             void* const         inValue) const
                                        { mGetFunction(inObject, inValue); }

    void                            SetValue(Object* const       inObject,
                                             const void* const   outValue) const
                                        { mSetFunction(inObject, outValue); }

private:
    const char*                     mName;
    const MetaType&                 mType;
    uint32_t                        mFlags;
    GetFunction                     mGetFunction;
    SetFunction                     mSetFunction;

    friend class Object;
};

/** Metadata for an Object-derived class. */
class MetaClass final : public MetaType
{
public:
    /** Type of an array of properties. */
    using PropertyArray           = std::vector<MetaProperty>;

    /** Type of the constructor function generated by ObjectGen. */
    using ConstructorFunction     = ObjPtr<> (*)();

public:
                                    MetaClass(const char*               inName,
                                              const size_t              inSize,
                                              const uint32_t            inTraits,
                                              const MetaClass* const    inParent,
                                              const ConstructorFunction inConstructor,
                                              const PropertyArray&      inProperties);
                                    ~MetaClass();

public:
    const MetaClass*                GetParent() const       { return static_cast<const MetaClass*>(mParent); }
    const PropertyArray&            GetProperties() const   { return mProperties; }

    bool                            IsConstructable() const;
    bool                            IsBaseOf(const MetaClass& inOther) const;

    /**
     * Constructs an instance of this class using its default constructor. The
     * class must be publically constructable, as indicated by IsConstructable().
     */
    ObjPtr<>                        Construct() const;

    template <typename T>
    ObjPtr<T>                       Construct() const;

    const MetaProperty*             LookupProperty(const char* const inName) const;

    /**
     * Get a list of constructable classes derived from this one (including
     * this class itself).
     */
    std::vector<const MetaClass*>   GetConstructableClasses(const bool inSorted = false) const;

    /**
     * To be used within a DebugWindow, implements a class selector UI for
     * constructable classes derived from this one (including the class itself).
     * Returns a class if one has been selected, otherwise returns null.
     */
    const MetaClass*                DebugUIClassSelector() const;

    static const MetaClass*         Lookup(const std::string &name);

    /**
     * For every known meta-class, executes the specified function on it. This
     * can be used, for example, to build up a list of meta-classes fulfilling
     * certain criteria.
     */
    static void                     Visit(const std::function<void (const MetaClass&)>& inFunction);

private:
    using PropertyMap             = HashMap<std::string, const MetaProperty*>;

private:
    /**
     * Constructs an object of this class using its default constructor. This
     * version allows construction even if the constructor is not public. The
     * primary use for this is deserialisation.
     */
    ObjPtr<>                        ConstructPrivate() const;

private:
    ConstructorFunction             mConstructor;
    const PropertyArray&            mProperties;

    /** Map of properties for fast lookup. */
    PropertyMap                     mPropertyMap;

    friend class Object;
    friend class Serialiser;
};

inline bool MetaClass::IsConstructable() const
{
    /* To the outside world, we only return true if the class is publically
     * constructable. The public Construct() method only works for classes for
     * which this is the case. Private construction is only used during
     * deserialisation, which is done internally. */
    return mTraits & kIsPublicConstructable;
}

/**
 * Object class.
 */

/**
 * Base class of all classes using the object system.
 *
 * The object system provides additional functionality on top of regular C++
 * classes. The primary feature is reflection, which is used for both automatic
 * (de)serialisation of properties, and for editing of properties. It also
 * allows the creation of new instances of Object-derived classes from the
 * reflection information, allowing for instance an object to be constructed
 * given a string containing the class name. In addition, all Objects are
 * reference counted.
 *
 * All classes which derive (directly or indirectly) from Object must be
 * annotated with the CLASS() macro. Class properties can be specified with the
 * PROPERTY() and VPROPERTY() macros. For further details, see the documentation
 * of those macros.
 *
 * In order for a class to be constructable through the object system (and
 * therefore able to be deserialised or created through the editor), it must
 * have a default constructor, i.e. one with no parameters. Other constructors
 * may exist as well, however these are not usable by the object system. To be
 * constructable by MetaClass::Construct(), this default constructor must also
 * be public. A non-public constructor can still be used to deserialise an
 * object, however.
 */
class Object : public RefCounted, Uncopyable
{
    DECLARE_STATIC_METACLASS("constructable": false);

public:
    /** Behaviour flags for DebugUIEditor(). */
    enum DebugUIEditorFlags : uint32_t
    {
        /**
         * Include editors for any child objects. Child objects are any
         * (non-Asset) Object-derived classes referred to by object properties,
         * plus any additional children returned by the custom editor callback.
         */
        kDebugUIEditor_IncludeChildren = (1 << 0),

        /**
         * Include a button to destroy the object. Whether destruction is
         * requested is indicated via the outDestroyObject return value.
         */
        kDebugUIEditor_AllowDestruction = (1 << 1),
    };

public:
    virtual const MetaClass&        GetMetaClass() const;

    template <typename T>
    bool                            GetProperty(const char* inName,
                                                T&          outValue) const;

    bool                            GetProperty(const char* const inName,
                                                const MetaType&   inType,
                                                void* const       outValue) const;

    template <typename T>
    bool                            SetProperty(const char* inName,
                                                const T&    inValue);

    bool                            SetProperty(const char* const inName,
                                                const MetaType&   inType,
                                                const void* const inValue);

    /**
     * To be used within a DebugWindow's Render() function, draws a UI to edit
     * the object. inFlags is a combination of DebugUIEditorFlags.
     */
    void                            DebugUIEditor(const uint32_t inFlags,
                                                  bool*          outDestroyObject = nullptr);

protected:
                                    Object() {}
                                    ~Object() {}

protected:
    /**
     * Serialises the object. The default implementation of this method will
     * automatically serialise all of the object's properties. Additional data
     * can be serialised by overridding this method to serialise it, as well as
     * Deserialise() to restore it. Overridden implementations *must* call their
     * parent class' implementation.
     */
    virtual void                    Serialise(Serialiser& inSerialiser) const;

    /**
     * Deserialises the object. For a class to be deserialisable, it must be
     * constructable (does not need to be publically), i.e. it must have a zero
     * argument constructor.
     *
     * When an object is being created from a serialised data file, an instance
     * of the class is first constructed using the zero-argument constructor.
     * It is the responsibility of this constructor to initialise default values
     * of all properties. Then, this method is called to restore serialised
     * data.
     *
     * The default implementation of this method will automatically deserialise
     * all of the object's properties. Additional data that was serialised by
     * Serialise() can be restored by overriding this method. Overridden
     * implementations *must* call their parent class' implementation.
     */
    virtual void                    Deserialise(Serialiser& inSerialiser);

    /**
     * Function to implement additional editor UI for DebugUIEditor() on top of
     * the basic class properties.
     *
     * If kDebugUIEditor_IncludeChildren is in inFlags and there are any extra
     * child objects that should have editors, then they can be added to the
     * ioChildren array.
     *
     * When this is called, the GUI is in a 2-column layout, expecting name in
     * the left column and value in the right.
     */
    virtual void                    CustomDebugUIEditor(const uint32_t        inFlags,
                                                        std::vector<Object*>& ioChildren);

    friend class Serialiser;
};

template <typename T>
inline bool Object::GetProperty(const char* inName,
                                T&          outValue) const
{
    return GetProperty(inName,
                       MetaType::Lookup<T>(),
                       std::addressof(outValue));
}

template <typename T>
inline bool Object::SetProperty(const char* inName,
                                const T&    inValue)
{
    return SetProperty(inName,
                       MetaType::Lookup<T>(),
                       std::addressof(inValue));
}

/**
 * Casts an Object pointer down the inheritance hierarchy. Similar to
 * dynamic_cast, but makes use of the object system's type information instead.
 * Only down-casts are allowed, up-casts should just be explicit conversions.
 * Returns null if the cast is successful.
 */
template <typename TargetPtr, typename Source>
inline TargetPtr object_cast(Source* const inObject)
{
    static_assert(std::is_pointer<TargetPtr>::value,
                  "Target type must be a pointer");

    using Target = typename std::remove_pointer<TargetPtr>::type;

    static_assert(std::is_base_of<Object, Source>::value,
                  "Source type must be derived from Object");
    static_assert(std::is_base_of<Source, Target>::value,
                  "Target type must be derived from source");
    static_assert(std::is_const<Target>::value == std::is_const<Source>::value &&
                      std::is_volatile<Target>::value == std::is_volatile<Source>::value,
                  "Target and source cv-qualifiers must be the same");

    return (Target::staticMetaClass.IsBaseOf(inObject->GetMetaClass()))
               ? static_cast<TargetPtr>(inObject)
               : nullptr;
}

/** Specialisation of object_cast for reference-counted pointers. */
template <typename TargetPtr, typename Source>
inline TargetPtr object_cast(const ObjPtr<Source>& inObject)
{
    return object_cast<typename TargetPtr::ReferencedType*>(inObject.Get());
}

template <typename T>
inline ObjPtr<T> MetaClass::Construct() const
{
    ObjPtr<> object = Construct();

    auto result = object_cast<ObjPtr<T>>(object);
    Assert(result);
    return result;
}
