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

/*
 * TODO:
 *  - Add version numbers to serialised files. This would allow us to handle
 *    changes in the serialised data. Need both an engine and a game version
 *    number, so that changes in both engine classes and game-specific classes
 *    can be handled separately.
 */

#pragma once

#include "Core/Math.h"

#include "Engine/Object.h"

namespace Detail
{
    #ifdef __clang__
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wundefined-inline"
    #endif

    /** Helper to identify a type which has a Serialise method. */
    template <typename T>
    class HasSerialise
    {
    private:
        template <typename U>
        static constexpr auto Test(int) ->
            typename std::is_same<
                decltype(std::declval<U>().Serialise(std::declval<Serialiser&>())),
                void
            >::type;

        template <typename U>
        static constexpr std::false_type Test(...);

    public:
        static constexpr bool value = decltype(Test<T>(0))::value;

    };

    /** Helper to identify a type which has a Deserialise method. */
    template <typename T>
    class HasDeserialise
    {
    private:
        template <typename U>
        static constexpr auto Test(int) ->
            typename std::is_same<
                decltype(std::declval<U>().Deserialise(std::declval<Serialiser&>())),
                void
            >::type;

        template <typename U>
        static constexpr std::false_type Test(...);

    public:
        static constexpr bool value = decltype(Test<T>(0))::value;
    };

    #ifdef __clang__
        #pragma clang diagnostic pop
    #endif
}

/**
 * This class is the base interface for classes which can serialise and
 * deserialise Object-derived classes. There are multiple implementations of
 * this class for different serialised file formats.
 *
 * The basic usage for serialisation is as follows:
 *
 *     JSONSerialiser serialiser;
 *     std::vector<uint8_t> data = serialiser.Serialise(object);
 *
 * For deserialisation:
 *
 *     JSONSerialiser serialiser;
 *     ObjectPtr<MyClass> object = serialiser.Deserialise<MyClass>(data);
 *
 * Internally, this uses Object::Serialise() and Object::Deserialise() to
 * (de)serialise the data. The base Object implementations of these methods
 * automatically (de)serialise all class properties. If any additional data
 * which is not stored in properties needs to be serialised, these methods can
 * be overridden to implement such behaviour.
 *
 * A serialised data file can contain multiple objects. This is for objects
 * which refer to some "child" objects. For example, a serialised Entity also
 * stores all of its Components. When this is done, the first object in the
 * file is the "primary" object, i.e. the object passed to Serialise() and the
 * one returned by deserialise(). Each object in the file has an index given by
 * the order they are defined in the file. Serialising a reference to an object
 * causes the object to be serialised, and the reference is stored as the ID of
 * the object in the file. A single object will only be serialised once within
 * the same file, i.e. adding 2 references to the same object (checked by
 * address) will only serialise 1 copy of it.
 *
 * An exception to this behaviour is for managed assets. Despite being just
 * objects, if a reference to an object derived from Asset is serialised and
 * the asset is managed, the asset path will be stored. Unmanaged assets will
 * be serialised to the file.
 */
class Serialiser
{
public:
    /** Type of a function to be called between construction and deserialisation. */
    using PostConstructFunction   = std::function<void (Object *)>;

public:
    virtual                         ~Serialiser() {}

    /**
     * Main interface.
     */
public:
    /**
     * Serialises the object into the file format implemented by this serialiser
     * instance. The return value is a binary data array which can be fed into
     * Deserialise() to reconstruct the object, written to a file to deserialise
     * later, etc.
     */
    virtual std::vector<uint8_t>    Serialise(const Object* const inObject) = 0;

    /**
     * Deserialises an object previously serialised in the format implemented
     * by this serialiser instance. Returns null on failure.
     */
    virtual ObjectPtr<>             Deserialise(const std::vector<uint8_t>& inData,
                                                const MetaClass&            inExpectedClass) = 0;

    /**
     * Deserialises an object previously serialised in the format implemented
     * by this serialiser instance. Returns null on failure.
     */
    template <typename T>
    ObjectPtr<T>                    Deserialise(const std::vector<uint8_t>& inData);

    /**
     * A function that will be called after construction of the object being
     * deserialised but before its Deserialise() method is called. This only
     * applies to the primary object in the serialised data, not any child
     * objects.
     */
    PostConstructFunction           postConstructFunction;

    /**
     * Interface used by (de)serialisation methods.
     */
public:
    /**
     * Begin/end a value group within the current scope. This can be used to
     * create a group of named values inside the current scope. This is useful
     * for example for nested structures (and is used by the implementations of
     * Read()/Write() for arbitrary structures).
     *
     * When serialising, this function creates a new group and makes it the
     * current scope. When deserialising, it looks for the specified group and
     * makes it the current scope.
     *
     * Each call to this function must be matched with a call to EndGroup() at
     * the end. As an example, using JSON serialisation, the following code:
     *
     *     serialiser.BeginGroup("foo");
     *     serialiser.Write("bar", mFoo.bar);
     *     serialiser.EndGroup();
     *
     * Gives the following:
     *
     *     "foo": {
     *         "bar": ...
     *     }
     *
     * During deserialisation, returns false if a group with the given name
     * cannot be found at the current scope. If false is returned, EndGroup()
     * should not be called. Does not fail for serialisation.
     *
     * If the current scope is an array, the name should be null. During
     * deserialisation, each consecutive call will return true if there are
     * still groups remaining in the array, or false if the end has been
     * reached.
     */
    virtual bool                    BeginGroup(const char* const inName = nullptr) = 0;
    virtual void                    EndGroup() = 0;

    /**
     * Begin/end a value array within the current scope. This can be used to
     * create a group of unnamed values inside the current scope, for example to
     * represent containers such as lists and arrays.
     *
     * When serialising, this function creates a new array and makes it the
     * current scope. When deserialising, it looks for the specified array and
     * makes it the current scope.
     *
     * Each call to this function must be matched with a call to EndArray() at
     * the end. Values should be read and written using Pop()/Push() rather than
     * Read()/Write(). Order is preserved, so items will be deserialised in the
     * order they were serialised. Pop() returns false to indicate that the end
     * of the array has been reached.
     *
     * During deserialisation, returns false if an array with the given name
     * cannot be found at the current scope. If false is returned, EndArray()
     * should not be called. Does not fail for serialisation.
     */
    virtual bool                    BeginArray(const char* const inName = nullptr) = 0;
    virtual void                    EndArray() = 0;

    void                            Write(const char* const inName, const bool& inValue);
    void                            Write(const char* const inName, const int8_t& inValue);
    void                            Write(const char* const inName, const uint8_t& inValue);
    void                            Write(const char* const inName, const int16_t& inValue);
    void                            Write(const char* const inName, const uint16_t& inValue);
    void                            Write(const char* const inName, const int32_t& inValue);
    void                            Write(const char* const inName, const uint32_t& inValue);
    void                            Write(const char* const inName, const int64_t& inValue);
    void                            Write(const char* const inName, const uint64_t& inValue);
    void                            Write(const char* const inName, const float& inValue);
    void                            Write(const char* const inName, const double& inValue);
    void                            Write(const char* const inName, const std::string& inValue);
    void                            Write(const char* const inName, const glm::vec2& inValue);
    void                            Write(const char* const inName, const glm::vec3& inValue);
    void                            Write(const char* const inName, const glm::vec4& inValue);
    void                            Write(const char* const inName, const glm::ivec2& inValue);
    void                            Write(const char* const inName, const glm::ivec3& inValue);
    void                            Write(const char* const inName, const glm::ivec4& inValue);
    void                            Write(const char* const inName, const glm::uvec2& inValue);
    void                            Write(const char* const inName, const glm::uvec3& inValue);
    void                            Write(const char* const inName, const glm::uvec4& inValue);
    void                            Write(const char* const inName, const glm::quat& inValue);

    template <typename T, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr>
    void                            Write(const char* const inName, const T& inValue)
                                        { Write(inName, MetaType::Lookup<T>(), &inValue); }

    /**
     * Serialises the object referred to by the given pointer if it has not
     * already been serialised in this file, and writes a reference to the
     * object within the serialised file. If the pointer refers to a managed
     * asset, then only a reference to that asset will be saved, rather than
     * including a serialised copy of the asset.
     */
    template <typename T, typename std::enable_if<std::is_base_of<Object, T>::value>::type* = nullptr>
    void                            Write(const char* const inName, const ObjectPtr<T>& inObject)
                                        { Write(inName, MetaType::Lookup<const ObjectPtr<T>>(), &inObject); }

    /**
     * Serialises the object referred to by the given pointer if it has not
     * already been serialised in this file, and writes a reference to the
     * object within the serialised file. If the pointer refers to a managed
     * asset, then only a reference to that asset will be saved, rather than
     * including a serialised copy of the asset.
     */
    template <typename T, typename std::enable_if<std::is_base_of<Object, T>::value>::type* = nullptr>
    void                            Write(const char* const inName, const T* inObject)
                                        { Write(inName, MetaType::Lookup<const T*>(), &inObject); }

    /**
     * This method can be used to write any type which provides a Serialise
     * method of the form:
     *
     *     void Serialise(Serialiser&) const;
     *
     * This method will begin a group with the given name, call the type's
     * Serialise() method, and end the group.
     */
    template <typename T, typename std::enable_if<Detail::HasSerialise<T>::value>::type* = nullptr>
    void                            Write(const char* const inName, const T& inValue);

    template <typename T>
    void                            Push(T&& inValue)
                                        { Write(nullptr, std::forward<T>(inValue)); }

    bool                            Read(const char* const inName, bool& outValue);
    bool                            Read(const char* const inName, int8_t& outValue);
    bool                            Read(const char* const inName, uint8_t& outValue);
    bool                            Read(const char* const inName, int16_t& outValue);
    bool                            Read(const char* const inName, uint16_t& outValue);
    bool                            Read(const char* const inName, int32_t& outValue);
    bool                            Read(const char* const inName, uint32_t& outValue);
    bool                            Read(const char* const inName, int64_t& outValue);
    bool                            Read(const char* const inName, uint64_t& outValue);
    bool                            Read(const char* const inName, float& outValue);
    bool                            Read(const char* const inName, double& outValue);
    bool                            Read(const char* const inName, std::string& outValue);
    bool                            Read(const char* const inName, glm::vec2& outValue);
    bool                            Read(const char* const inName, glm::vec3& outValue);
    bool                            Read(const char* const inName, glm::vec4& outValue);
    bool                            Read(const char* const inName, glm::ivec2& outValue);
    bool                            Read(const char* const inName, glm::ivec3& outValue);
    bool                            Read(const char* const inName, glm::ivec4& outValue);
    bool                            Read(const char* const inName, glm::uvec2& outValue);
    bool                            Read(const char* const inName, glm::uvec3& outValue);
    bool                            Read(const char* const inName, glm::uvec4& outValue);
    bool                            Read(const char* const inName, glm::quat& outValue);

    template <typename T, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr>
    bool                            Read(const char* const inName, T& outValue)
                                        { return Read(inName, MetaType::Lookup<T>(), &outValue); }

    /**
     * Deserialises the specified object if it has not already been deserialised
     * from this file, and returns a reference to the object. If the object
     * could not be found, the supplied ObjectPtr is not changed.
     */
    template <typename T, typename std::enable_if<std::is_base_of<Object, T>::value>::type* = nullptr>
    bool                            Read(const char* const inName, ObjectPtr<T>& outObject)
                                        { return Read(inName, MetaType::Lookup<ObjectPtr<T>>(), &outObject); }

    /**
     * Deserialises the specified object if it has not already been deserialised
     * from this file, and returns a pointer to the object. If the object could
     * not be found, the supplied pointer is not changed.
     *
     * This is a non-reference counting pointer, so users should be sure that
     * the object will have a proper reference elsewhere. The object will be
     * kept alive until the end of the Deserialise() call; if no references
     * remain after that then the object will be freed and this pointer will be
     * invalid.
     */
    template <typename T, typename std::enable_if<std::is_base_of<Object, T>::value>::type* = nullptr>
    bool                            Read(const char* const inName, T*& outObject)
                                        { return Read(inName, MetaType::Lookup<T*>(), &outObject); }

    /**
     * This method can be used to read any type which provides a deserialise
     * method of the form:
     *
     *     void Deserialise(Serialiser&);
     *
     * This method will begin a group with the given name, call the type's
     * Deserialise() method, and end the group.
     */
    template <typename T, typename std::enable_if<Detail::HasDeserialise<T>::value>::type * = nullptr>
    bool                            Read(const char* const inName, T& outValue);

    template <typename T>
    bool                            Pop(T&& outValue)
                                        { return Read(nullptr, std::forward<T>(outValue)); }

protected:
                                    Serialiser() {}

protected:
    virtual void                    Write(const char* const inName,
                                          const MetaType&   inType,
                                          const void* const inValue) = 0;

    virtual bool                    Read(const char* const inName,
                                         const MetaType&   inType,
                                         void* const       outValue) = 0;

    void                            SerialiseObject(const Object* const inObject);

    bool                            DeserialiseObject(const char* const inClassName,
                                                      const MetaClass&  inMetaClass,
                                                      const bool        inIsPrimary,
                                                      ObjectPtr<>&      outObject);

    friend class Object;
};

template <typename T>
inline ObjectPtr<T> Serialiser::Deserialise(const std::vector<uint8_t>& inData)
{
    ObjectPtr<> object = Deserialise(inData, T::staticMetaClass);
    return object.StaticCast<T>();
}

template <typename T, typename std::enable_if<Detail::HasSerialise<T>::value>::type*>
inline void Serialiser::Write(const char* const inName,
                              const T&          inValue)
{
    BeginGroup(inName);
    inValue.Serialise(*this);
    EndGroup();
}

template <typename T, typename std::enable_if<Detail::HasDeserialise<T>::value>::type*>
inline bool Serialiser::Read(const char* const inName,
                             T&                outValue)
{
    if (BeginGroup(inName))
    {
        outValue.Deserialise(*this);
        EndGroup();
        return true;
    }
    else
    {
        return false;
    }
}
