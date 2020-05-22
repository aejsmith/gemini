/*
 * Copyright (C) 2018-2020 Alex Smith
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

#include "Core/Filesystem.h"
#include "Core/String.h"

#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clang-c/Index.h>

#include <mustache.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#if GEMINI_PLATFORM_WIN32
    #include "Core/Win32/Win32.h"
#endif

#include "ObjectGen.mustache.h"

using namespace kainjow;

class ParsedTranslationUnit;

class ParsedDecl
{
public:
    using VisitFunction           = std::function<void (CXCursor, CXCursorKind)>;

public:
                                    ParsedDecl(CXCursor          cursor,
                                               ParsedDecl* const parent       = nullptr,
                                               const bool        nameFromType = false);

    virtual                         ~ParsedDecl() {}

    bool                            IsFromMainFile() const;
    ParsedTranslationUnit*          GetTranslationUnit();

    virtual mustache::data          Generate() const
                                        { return mustache::data(); }

    virtual void                    Dump(const unsigned depth) const = 0;

    static void                     VisitChildren(CXCursor          cursor,
                                                  ParsedDecl* const decl);

    static void                     VisitChildren(CXCursor             cursor,
                                                  const VisitFunction& function);

public:
    CXCursor                        mCursor;
    ParsedDecl*                     mParent;
    std::string                     mName;
    bool                            mIsAnnotated;

protected:
    virtual bool                    HandleAnnotation(const std::string&         type,
                                                     const rapidjson::Document& attributes)
                                        { return false; }

    virtual void                    HandleChild(CXCursor     cursor,
                                                CXCursorKind kind) {}

private:
    static CXChildVisitResult       VisitDeclCallback(CXCursor     cursor,
                                                      CXCursor     parent,
                                                      CXClientData data);

    static CXChildVisitResult       VisitFunctionCallback(CXCursor     cursor,
                                                          CXCursor     parent,
                                                          CXClientData data);

};

class ParsedProperty : public ParsedDecl
{
public:
                                    ParsedProperty(CXCursor          cursor,
                                                   ParsedDecl* const parent);

    mustache::data                  Generate() const override;
    void                            Dump(const unsigned depth) const override;

protected:
    bool                            HandleAnnotation(const std::string&         type,
                                                     const rapidjson::Document& attributes) override;

private:
    std::string                     mType;
    std::string                     mGetFunction;
    std::string                     mSetFunction;

    /** Behaviour flags. */
    bool                            mTransient;

};

using ParsedPropertyList = std::list<std::unique_ptr<ParsedProperty>>;

class ParsedClass : public ParsedDecl
{
public:
                                    ParsedClass(CXCursor          cursor,
                                                ParsedDecl* const parent);

    bool                            IsObject() const;
    bool                            IsConstructable() const;
    bool                            IsPublicConstructable() const;

    mustache::data                  Generate() const override;
    void                            Dump(const unsigned depth) const override;

public:
    bool                            mIsObjectDerived;
    CX_CXXAccessSpecifier           mDestructorAccess;

protected:
    bool                            HandleAnnotation(const std::string&         type,
                                                     const rapidjson::Document& attributes) override;

    void                            HandleChild(CXCursor     cursor,
                                                CXCursorKind kind) override;

private:
    /** Whether the class is constructable. */
    enum Constructability
    {
        kConstructability_Default,      /**< No constructors have yet been declared. */
        kConstructability_Public,       /**< Publically, the default when no constructor is declared. */
        kConstructability_Private,      /**< Private or protected. Only usable for deserialisation. */
        kConstructability_None,         /**< None, if no suitable constructor found. */
        kConstructability_ForcedNone,   /**< Forced off by attribute. */
    };

private:
    ParsedClass*                    mParentClass;
    ParsedPropertyList              mProperties;
    Constructability                mConstructable;

    /** Temporary state used while parsing. */
    bool                            mOnMetaClass;

};

using ParsedClassMap = std::map<std::string, std::unique_ptr<ParsedClass>>;

class ParsedEnum : public ParsedDecl
{
public:
    mustache::data                  Generate() const override;
    void                            Dump(const unsigned depth) const override;

    static void                     Create(CXCursor          cursor,
                                           ParsedDecl* const parent);

protected:
    bool                            HandleAnnotation(const std::string&         type,
                                                     const rapidjson::Document& attributes) override;

    void                            HandleChild(CXCursor     cursor,
                                                CXCursorKind kind) override;

private:
    using EnumConstant            = std::pair<std::string, long long>;
    using EnumConstantList        = std::list<EnumConstant>;

private:
                                    ParsedEnum(CXCursor    cursor,
                                               ParsedDecl* parent);

private:
    bool                            mShouldGenerate;
    EnumConstantList                mConstants;

};

using ParsedEnumMap = std::map<std::string, std::unique_ptr<ParsedEnum>>;

class ParsedTranslationUnit : public ParsedDecl
{
public:
    explicit                        ParsedTranslationUnit(CXCursor cursor);

    mustache::data                  Generate() const override;
    void                            Dump(const unsigned depth) const override;

public:
    /** List of child classes. */
    ParsedClassMap                  mClasses;

    /** List of child enumerations (including ones nested within classes). */
    ParsedEnumMap                   mEnums;

protected:
    void                            HandleChild(CXCursor     cursor,
                                                CXCursorKind kind) override;

};

static bool gParseErrorOccurred = false;

static void ParseError(CXCursor          cursor,
                       const char* const format,
                       ...)
{
    CXSourceLocation location = clang_getCursorLocation(cursor);
    CXFile file;
    unsigned line, column;
    clang_getSpellingLocation(location, &file, &line, &column, nullptr);
    CXString fileName = clang_getFileName(file);

    fprintf(stderr, "%s:%u:%u: error: ", clang_getCString(fileName), line, column);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");

    gParseErrorOccurred = true;
}

static void ParseWarning(CXCursor          cursor,
                         const char* const format,
                         ...)
{
    CXSourceLocation location = clang_getCursorLocation(cursor);
    CXFile file;
    unsigned line, column;
    clang_getSpellingLocation(location, &file, &line, &column, nullptr);
    CXString fileName = clang_getFileName(file);

    fprintf(stderr, "%s:%u:%u: warning: ", clang_getCString(fileName), line, column);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

/**
 * In the generated code we base the name of some of the variables we define
 * on the name of the class or enum. For a class or enum that is nested in a
 * namespace or inside another class, the name is of the form "Foo::Bar". This
 * cannot be directly used to name a variable, e.g. "Foo::Bar_data". This
 * function solves this by replacing "::" in the name string with "_" to give
 * a name suitable for naming our generated variables.
 */
static std::string MangleName(const std::string& name)
{
    std::string mangled(name);

    size_t pos;
    while ((pos = mangled.find("::")) != std::string::npos)
    {
        mangled.replace(pos, 2, "_");
    }

    return mangled;
}

ParsedDecl::ParsedDecl(CXCursor          cursor,
                       ParsedDecl* const parent,
                       const bool        nameFromType) :
    mCursor      (cursor),
    mParent      (parent),
    mIsAnnotated (false)
{
    CXString name;

    if (nameFromType)
    {
        CXType type = clang_getCursorType(mCursor);
        name = clang_getTypeSpelling(type);
    }
    else
    {
        name = clang_getCursorSpelling(mCursor);
    }

    mName = clang_getCString(name);
    clang_disposeString(name);
}

bool ParsedDecl::IsFromMainFile() const
{
    return clang_Location_isFromMainFile(clang_getCursorLocation(mCursor));
}

ParsedTranslationUnit* ParsedDecl::GetTranslationUnit()
{
    ParsedDecl* decl = this;

    while (decl->mParent)
    {
        decl = decl->mParent;
    }

    return static_cast<ParsedTranslationUnit*>(decl);
}

static bool ParseAnnotation(CXCursor             cursor,
                            std::string&         outType,
                            rapidjson::Document& outAttributes)
{
    CXString str = clang_getCursorSpelling(cursor);
    std::string annotation(clang_getCString(str));
    clang_disposeString(str);

    std::vector<std::string> tokens;
    StringUtils::Tokenize(annotation, tokens, ":", 3, false);

    if (tokens[0].compare("gemini"))
    {
        /* Don't raise an error for annotations that aren't marked as being for
         * us, could be annotations for other reasons. */
        return false;
    }
    else if (tokens.size() != 3)
    {
        ParseError(cursor, "malformed annotation");
        return false;
    }

    outType = tokens[1];

    std::string json = std::string("{") + tokens[2] + std::string("}");

    outAttributes.Parse(json.c_str());

    if (outAttributes.HasParseError())
    {
        ParseError(cursor,
                   "parse error in attributes (at %zu): %s",
                   outAttributes.GetErrorOffset() - 1,
                   rapidjson::GetParseError_En(outAttributes.GetParseError()));

        return false;
    }

    return true;
}

CXChildVisitResult ParsedDecl::VisitDeclCallback(CXCursor     cursor,
                                                 CXCursor     parent,
                                                 CXClientData data)
{
    auto currentDecl = reinterpret_cast<ParsedDecl*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);
    if (kind == CXCursor_AnnotateAttr)
    {
        std::string type;
        rapidjson::Document attributes;
        if (!ParseAnnotation(cursor, type, attributes))
        {
            return CXChildVisit_Continue;
        }

        if (currentDecl->HandleAnnotation(type, attributes))
        {
            currentDecl->mIsAnnotated = true;
        }
        else
        {
            ParseError(cursor, "unexpected '%s' annotation", type.c_str());
        }
    }
    else
    {
        currentDecl->HandleChild(cursor, kind);
    }

    return CXChildVisit_Continue;
}

void ParsedDecl::VisitChildren(CXCursor    cursor,
                               ParsedDecl* decl)
{
    clang_visitChildren(cursor, VisitDeclCallback, decl);
}

CXChildVisitResult ParsedDecl::VisitFunctionCallback(CXCursor     cursor,
                                                     CXCursor     parent,
                                                     CXClientData data)
{
    auto& function = *reinterpret_cast<const VisitFunction*>(data);

    CXCursorKind kind = clang_getCursorKind(cursor);
    function(cursor, kind);

    return CXChildVisit_Continue;
}

void ParsedDecl::VisitChildren(CXCursor             cursor,
                               const VisitFunction& function)
{
    clang_visitChildren(cursor,
                        VisitFunctionCallback,
                        const_cast<VisitFunction*>(&function));
}

ParsedProperty::ParsedProperty(CXCursor          cursor,
                               ParsedDecl* const parent) :
    ParsedDecl (cursor, parent),
    mTransient (false)
{
    /* Remove prefixes from virtual property names. */
    if (mName.substr(0, 6) == "vprop_")
    {
        mName = mName.substr(6);
    }

    /* Get the property type. */
    CXType type  = clang_getCursorType(cursor);
    CXString str = clang_getTypeSpelling(type);
    mType        = clang_getCString(str);
    clang_disposeString(str);
}

bool ParsedProperty::HandleAnnotation(const std::string&         type,
                                      const rapidjson::Document& attributes)
{
    if (type.compare("property") != 0)
    {
        return false;
    }

    /* Now that we know that we are really a property, if our type is an enum,
     * mark that enum for code generation. */
    CXType propertyType       = clang_getCursorType(mCursor);
    CXCursor propertyTypeDecl = clang_getTypeDeclaration(propertyType);

    if (clang_getCursorKind(propertyTypeDecl) == CXCursor_EnumDecl)
    {
        const ParsedTranslationUnit* translationUnit = GetTranslationUnit();

        auto it = translationUnit->mEnums.find(mType);
        if (it == translationUnit->mEnums.end())
        {
            ParseError(mCursor,
                       "enum '%s' for property '%s' must be marked with ENUM()",
                       mType.c_str(),
                       mName.c_str());

            return true;
        }
    }

    auto parent = static_cast<ParsedClass*>(mParent);

    if (!parent->mIsObjectDerived)
    {
        ParseError(mCursor,
                   "'property' annotation on field '%s' in non-Object class '%s'",
                   mName.c_str(),
                   mParent->mName.c_str());

        return true;
    }

    static const char* kGetAttribute       = "get";
    static const char* kSetAttribute       = "set";
    static const char* kTransientAttribute = "transient";

    if (attributes.HasMember(kGetAttribute))
    {
        const rapidjson::Value& value = attributes[kGetAttribute];

        if (!value.IsString())
        {
            ParseError(mCursor, "'%s' attribute must be a string", kGetAttribute);
            return true;
        }

        mGetFunction = value.GetString();
    }

    if (attributes.HasMember(kSetAttribute))
    {
        const rapidjson::Value& value = attributes[kSetAttribute];

        if (!value.IsString())
        {
            ParseError(mCursor, "'%s' attribute must be a string", kSetAttribute);
            return true;
        }

        mSetFunction = value.GetString();
    }

    if (mGetFunction.empty() != mSetFunction.empty())
    {
        ParseError(mCursor, "both 'get' and 'set' or neither of them must be specified");
        return true;
    }

    if (attributes.HasMember(kTransientAttribute))
    {
        const rapidjson::Value& value = attributes[kTransientAttribute];

        if (!value.IsBool())
        {
            ParseError(mCursor, "'%s' attribute must be a boolean", kTransientAttribute);
            return true;
        }

        mTransient = value.GetBool();
    }

    if (clang_getCXXAccessSpecifier(mCursor) != CX_CXXPublic)
    {
        ParseError(mCursor, "property '%s' must be public", mName.c_str());
        return true;
    }

    const bool isVirtual = clang_getCursorKind(mCursor) == CXCursor_VarDecl;

    if (isVirtual)
    {
        if (mGetFunction.empty())
        {
            /* These require getters and setters. If they are omitted, default
             * names are used based on the property name (see Object.h). */
            std::string name = static_cast<char>(toupper(mName[0])) + mName.substr(1);

            mGetFunction = std::string("Get") + name;
            mSetFunction = std::string("Set") + name;
        }
    }
    else if (!mGetFunction.empty())
    {
        /* This makes no sense - code can directly access/modify the property
         * so usage of getter/setter methods should not be required. */
        ParseError(mCursor, "public properties cannot have getter/setter methods");
        return true;
    }

    return true;
}

mustache::data ParsedProperty::Generate() const
{
    mustache::data data;

    /* Generate a flags string. */
    std::string flags;
    auto AddFlag =
        [&] (const char* flag)
        {
            if (!flags.empty())
            {
                flags += " | ";
            }

            flags += "MetaProperty::";
            flags += flag;
        };

    if (mTransient)
    {
        AddFlag("kTransient");
    }

    if (flags.empty())
    {
        flags = "0";
    }

    data.set("propertyName", mName);
    data.set("propertyType", mType);
    data.set("propertyFlags", flags);

    if (!mGetFunction.empty())
    {
        data.set("propertyGet", mGetFunction);
        data.set("propertySet", mSetFunction);
    }

    return data;
}

void ParsedProperty::Dump(const unsigned depth) const
{
    printf("%-*sProperty '%s' (type '%s', get '%s', set '%s')\n",
           depth * 2, "",
           mName.c_str(),
           mType.c_str(),
           mGetFunction.c_str(),
           mSetFunction.c_str());
}

ParsedClass::ParsedClass(CXCursor          cursor,
                         ParsedDecl* const parent) :
    ParsedDecl        (cursor, parent, true),
    mIsObjectDerived  (mName.compare("Object") == 0),
    mDestructorAccess (CX_CXXPublic),
    mParentClass      (nullptr),
    mConstructable    (kConstructability_Default),
    mOnMetaClass      (false)
{
}

bool ParsedClass::IsObject() const
{
    if (mIsAnnotated && mIsObjectDerived)
    {
        return true;
    }
    else if (mIsObjectDerived)
    {
        ParseError(mCursor,
                   "Object-derived class '%s' missing 'class' annotation; CLASS() macro missing?",
                   mName.c_str());
    }

    return false;
}

bool ParsedClass::IsConstructable() const
{
    switch (mConstructable)
    {
        case kConstructability_Default:
        case kConstructability_Public:
        case kConstructability_Private:
            return true;

        default:
            return false;

    }
}

bool ParsedClass::IsPublicConstructable() const
{
    switch (mConstructable)
    {
        case kConstructability_Default:
        case kConstructability_Public:
            return true;

        default:
            return false;
    }
}

bool ParsedClass::HandleAnnotation(const std::string&         type,
                                   const rapidjson::Document& attributes)
{
    if (!mOnMetaClass || type.compare("class") != 0)
    {
        return false;
    }

    if (!mIsObjectDerived)
    {
        ParseError(mCursor,
                   "'class' annotation on non-Object class '%s'",
                   mName.c_str());
    }

    if (attributes.HasMember("constructable"))
    {
        const rapidjson::Value& value = attributes["constructable"];

        if (!value.IsBool())
        {
            ParseError(mCursor, "'constructable' attribute must be a boolean");
            return true;
        }

        const bool constructable = value.GetBool();
        if (constructable)
        {
            ParseError(mCursor, "constructability cannot be forced on, only off");
            return true;
        }

        mConstructable = kConstructability_ForcedNone;
    }

    return true;
}

void ParsedClass::HandleChild(CXCursor     cursor,
                              CXCursorKind kind)
{
    if (mOnMetaClass)
    {
        return;
    }

    switch (kind)
    {
        case CXCursor_CXXBaseSpecifier:
        {
            /* Check if this class is derived from Object. This gives us the
             * fully-qualified name (with all namespaces) regardless of whether
             * it was specified that way in the source. */
            CXType type  = clang_getCursorType(cursor);
            CXString str = clang_getTypeSpelling(type);
            std::string typeName(clang_getCString(str));
            clang_disposeString(str);

            /* The translation unit records all Object-derived classes seen,
             * even those outside the main file. Therefore, we look for the
             * base class name in there, and if it matches one of those, then
             * we are an Object-derived class as well. */
            const ParsedTranslationUnit* translationUnit = GetTranslationUnit();

            auto it = translationUnit->mClasses.find(typeName);
            if (it != translationUnit->mClasses.end())
            {
                /* If mIsObjectDerived is already set to true, then we have
                 * multiple inheritance, which is unsupported. */
                if (mIsObjectDerived)
                {
                    ParseError(cursor,
                               "Inheritance from multiple Object-derived classes is unsupported (on class '%s')",
                               mName.c_str());
                }

                mIsObjectDerived = true;
                mParentClass     = it->second.get();
            }

            break;
        }

        case CXCursor_Constructor:
        {
            /* Ignore if forced to be non-constructable. */
            if (mConstructable == kConstructability_ForcedNone)
            {
                break;
            }

            /* Determine the number of parameters to this constructor. */
            unsigned numParams = 0;
            VisitChildren(
                cursor,
                [&numParams] (CXCursor, CXCursorKind kind)
                {
                    if (kind == CXCursor_ParmDecl)
                    {
                        numParams++;
                    }
                });

            /* Only constructors with no parameters are suitable. */
            if (numParams == 0)
            {
                if (clang_getCXXAccessSpecifier(cursor) == CX_CXXPublic)
                {
                    mConstructable = kConstructability_Public;
                }
                else
                {
                    mConstructable = kConstructability_Private;
                }
            }
            else
            {
                /* If no other constructors have been seen so far, mark as
                 * non-constructable. */
                if (mConstructable == kConstructability_Default)
                {
                    mConstructable = kConstructability_None;
                }
            }

            break;
        }

        case CXCursor_Destructor:
        {
            mDestructorAccess = clang_getCXXAccessSpecifier(cursor);
            break;
        }

        case CXCursor_VarDecl:
        {
            /* Static class variables fall under VarDecl. The class annotation
             * is applied to the staticMetaClass member, so if we have that
             * variable, then descend onto children keeping the same current
             * declaration so we see the annotation below. */
            CXString str = clang_getCursorSpelling(cursor);
            std::string typeName(clang_getCString(str));
            clang_disposeString(str);

            if (!typeName.compare("staticMetaClass"))
            {
                mOnMetaClass = true;
                VisitChildren(cursor, this);
                mOnMetaClass = false;
                break;
            }

            /* Fall through to check for virtual properties. */
        }

        case CXCursor_FieldDecl:
        {
            /* FieldDecl is an instance variable. Look for properties. */
            std::unique_ptr<ParsedProperty> parsedProperty(new ParsedProperty(cursor, this));
            VisitChildren(cursor, parsedProperty.get());

            if (parsedProperty->mIsAnnotated)
            {
                mProperties.emplace_back(std::move(parsedProperty));
            }

            break;
        }

        case CXCursor_CXXMethod:
        {
            /* Classes with pure virtual methods are not constructable.
             * TODO: This does not handle a class which is abstract because a
             * parent class has virtual methods that it does not override.
             * libclang doesn't appear to have an easy way to identify this, so
             * for now don't handle it. If it does become a problem it can be
             * worked around using the constructable attribute. */
            if (clang_CXXMethod_isPureVirtual(cursor))
            {
                mConstructable = kConstructability_ForcedNone;
            }

            break;
        }

        case CXCursor_EnumDecl:
        {
            ParsedEnum::Create(cursor, this);
            break;
        }

        default:
        {
            break;
        }
    }
}

mustache::data ParsedClass::Generate() const
{
    mustache::data data;

    data.set("name", mName);
    data.set("mangledName", MangleName(mName));

    if (mParentClass)
    {
        data.set("parent", mParentClass->mName);
    }

    if (IsConstructable())
    {
        data.set("isConstructable", mustache::data::type::bool_true);
    }

    if (IsPublicConstructable())
    {
        data.set("isPublicConstructable", mustache::data::type::bool_true);
    }

    mustache::data properties(mustache::data::type::list);
    for (const std::unique_ptr<ParsedProperty> &parsedProperty : mProperties)
    {
        properties.push_back(parsedProperty->Generate());
    }

    data.set("properties", properties);

    return data;
}

void ParsedClass::Dump(const unsigned depth) const
{
    printf("%-*sClass '%s' (", depth * 2, "", mName.c_str());

    if (mParentClass)
    {
        printf("parent '%s', ", mParentClass->mName.c_str());
    }

    printf("constructable %d %d)\n", IsConstructable(), IsPublicConstructable());

    for (const std::unique_ptr<ParsedProperty> &parsedProperty : mProperties)
    {
        parsedProperty->Dump(depth + 1);
    }
}

ParsedEnum::ParsedEnum(CXCursor          cursor,
                       ParsedDecl* const parent) :
    ParsedDecl      (cursor, parent, true),
    mShouldGenerate (false)
{
}

void ParsedEnum::Create(CXCursor          cursor,
                        ParsedDecl* const parent)
{
    if (!clang_isCursorDefinition(cursor))
    {
        return;
    }

    /* We don't handle anonymous enums. There is no function that specifically
     * identifies this, so the way we do this is to check if the cursor
     * spelling is empty. Have to do this separately rather than checking the
     * name obtained by the constructor because that gets the type spelling
     * which is not empty for an anonymous enum. */
    CXString name        = clang_getCursorSpelling(cursor);
    const bool anonymous = std::strlen(clang_getCString(name)) == 0;
    clang_disposeString(name);
    if (anonymous)
    {
        return;
    }

    std::unique_ptr<ParsedEnum> parsedEnum(new ParsedEnum(cursor, parent));

    VisitChildren(cursor, parsedEnum.get());

    if (parsedEnum->mShouldGenerate)
    {
        parent->GetTranslationUnit()->mEnums.insert(std::make_pair(parsedEnum->mName, std::move(parsedEnum)));
    }
}

bool ParsedEnum::HandleAnnotation(const std::string&         type,
                                  const rapidjson::Document& attributes)
{
    if (type.compare("enum") != 0)
    {
        return false;
    }

    mShouldGenerate = true;
    return true;
}

void ParsedEnum::HandleChild(CXCursor     cursor,
                             CXCursorKind kind)
{
    if (kind == CXCursor_EnumConstantDecl)
    {
        long long value = clang_getEnumConstantDeclValue(cursor);

        CXString str = clang_getCursorSpelling(cursor);
        std::string name(clang_getCString(str));
        clang_disposeString(str);

        /* Shorten names based on our naming convention,
         * e.g. for enum EnumName, kEnumName_Foo -> Foo. */
        const std::string prefix("k" + mName + "_");
        if (name.rfind(prefix, 0) == 0)
        {
            name = name.substr(prefix.length());
        }

        /* Ignore special count values, again based on our naming convention,
         * e.g. for enum EnumName, ignore kEnumNameCount. These are typically
         * used in code for knowing the maximum value of an enum and are not
         * actually a valid value. */
        const std::string countName("k" + mName + "Count");
        if (name != countName)
        {
            mConstants.push_back(std::make_pair(name, value));
        }
    }
}

mustache::data ParsedEnum::Generate() const
{
    mustache::data data;

    data.set("name", mName);
    data.set("mangledName", MangleName(mName));

    mustache::data constants(mustache::data::type::list);
    for (const EnumConstant& constant : mConstants)
    {
        std::stringstream valueStr;
        valueStr << constant.second;

        mustache::data constantData;
        constantData.set("constantName", constant.first);
        constantData.set("constantValue", valueStr.str());
        constants.push_back(constantData);
    }

    data.set("constants", constants);

    return data;
}

void ParsedEnum::Dump(const unsigned depth) const
{
    printf("%-*sEnum '%s'\n", depth * 2, "", mName.c_str());

    for (auto& pair : mConstants)
    {
        printf("%-*s'%s' = %lld\n", (depth + 1) * 2, "", pair.first.c_str(), pair.second);
    }
}

ParsedTranslationUnit::ParsedTranslationUnit(CXCursor cursor) :
    ParsedDecl (cursor)
{
}

void ParsedTranslationUnit::HandleChild(CXCursor     cursor,
                                        CXCursorKind kind)
{
    switch (kind)
    {
        case CXCursor_Namespace:
            /* Descend into namespaces. */
            VisitChildren(cursor, this);
            break;

        case CXCursor_ClassDecl:
        case CXCursor_StructDecl:
            /* Ignore forward declarations. */
            if (clang_isCursorDefinition(cursor))
            {
                std::unique_ptr<ParsedClass> parsedClass(new ParsedClass(cursor, this));
                VisitChildren(cursor, parsedClass.get());

                if (parsedClass->IsObject())
                {
                    if (parsedClass->mDestructorAccess == CX_CXXPublic)
                    {
                        ParseWarning(parsedClass->mCursor,
                                     "Object-derived class has public destructor; this should be hidden with reference counting used instead");
                    }

                    mClasses.insert(std::make_pair(parsedClass->mName, std::move(parsedClass)));
                }
            }

            break;

        case CXCursor_EnumDecl:
            ParsedEnum::Create(cursor, this);
            break;

        default:
            break;

    }
}

mustache::data ParsedTranslationUnit::Generate() const
{
    mustache::data classes(mustache::data::type::list);
    for (auto& it : mClasses)
    {
        const std::unique_ptr<ParsedClass>& parsedClass = it.second;

        if (parsedClass->IsFromMainFile())
        {
            classes.push_back(parsedClass->Generate());
        }
    }

    mustache::data enums(mustache::data::type::list);
    for (auto& it : mEnums)
    {
        const std::unique_ptr<ParsedEnum>& parsedEnum = it.second;

        if (parsedEnum->IsFromMainFile())
        {
            enums.push_back(parsedEnum->Generate());
        }
    }

    mustache::data data;
    data.set("classes", classes);
    data.set("enums", enums);
    return data;
}

void ParsedTranslationUnit::Dump(const unsigned depth) const
{
    printf("%-*sTranslationUnit '%s'\n", depth * 2, "", mName.c_str());

    for (auto& it : mClasses)
    {
        const std::unique_ptr<ParsedClass>& parsedClass = it.second;

        if (parsedClass->IsFromMainFile())
        {
            parsedClass->Dump(depth + 1);
        }
    }

    for (auto& it : mEnums)
    {
        const std::unique_ptr<ParsedEnum>& parsedEnum = it.second;

        if (parsedEnum->IsFromMainFile())
        {
            parsedEnum->Dump(depth + 1);
        }
    }
}

static void Usage(const char* programName)
{
    printf("Usage: %s [options...] <source> <output>\n", programName);
    printf("\n");
    printf("Options:\n");
    printf("  -h            Display this help\n");
    printf("  -d            Dump parsed information, do not generate code\n");
    printf("  -D<define>    Preprocessor definition (as would be passed to clang)\n");
    printf("  -I<path>      Preprocessor include path (as would be passed to clang)\n");
    printf("  -s            Generate standalone code, which does not include the source file\n");
    printf("  -e            Ignore parse errors, generate empty output if any occur\n");
}

int main(const int          argc,
         char* const* const argv)
{
    std::vector<const char*> clangArgs;

    bool dump         = false;
    bool standalone   = false;
    bool ignoreErrors = false;

    /* Parse arguments. */
    int opt;
    while ((opt = getopt(argc, argv, "hdD:I:se")) != -1)
    {
        switch (opt)
        {
            case 'h':
                Usage(argv[0]);
                return EXIT_SUCCESS;

            case 'd':
                dump = true;
                break;

            case 'D':
                clangArgs.push_back("-D");
                clangArgs.push_back(optarg);
                break;

            case 'I':
                clangArgs.push_back("-I");
                clangArgs.push_back(optarg);
                break;

            case 's':
                standalone = true;
                break;

            case 'e':
                ignoreErrors = true;
                break;

            default:
                return EXIT_FAILURE;

        }
    }

    if (argc - optind != 2)
    {
        Usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* sourceFile = argv[optind];
    const char* outputFile = argv[optind + 1];

    /* Open the output file. This must be done first for standalone mode, so
     * that the generated file included by the source file exists. The wrapper
     * ensures that it is deleted if we fail. */
    struct OutputStream : std::ofstream
    {
        explicit OutputStream(const char* path) :
            std::ofstream (path, std::ofstream::out | std::ofstream::trunc),
            file          (path)
        {}

        ~OutputStream()
        {
            if (is_open())
            {
                close();
                std::remove(this->file);
            }
        }

        const char* file;
    };

    OutputStream outputStream(outputFile);
    if (!outputStream)
    {
        fprintf(stderr, "%s: Failed to open '%s': %s\n", argv[0], outputFile, strerror(errno));
        return EXIT_FAILURE;
    }

    /* Source code is C++14, and define a macro to indicate we are the object
     * compiler. */
    clangArgs.push_back("-x");
    clangArgs.push_back("c++");
    clangArgs.push_back("-std=c++14");
    clangArgs.push_back("-DGEMINI_OBJGEN=1");
    #if GEMINI_PLATFORM_WIN32
        clangArgs.push_back("-fms-compatibility-version=19");
    #endif

    /* Create an index with diagnostic output disabled. */
    CXIndex index = clang_createIndex(1, 0);

    /* Parse the source file. */
    CXTranslationUnit unit =
        clang_parseTranslationUnit(index,
                                   sourceFile,
                                   &clangArgs[0],
                                   clangArgs.size(),
                                   nullptr,
                                   0,
                                   CXTranslationUnit_Incomplete | CXTranslationUnit_SkipFunctionBodies);
    if (!unit)
    {
        fprintf(stderr, "%s: Error creating translation unit\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Check for errors. */
    const unsigned numDiags = clang_getNumDiagnostics(unit);
    bool hadError           = false;

    for (unsigned i = 0; i < numDiags; i++)
    {
        CXDiagnostic diag = clang_getDiagnostic(unit, i);

        if (clang_getDiagnosticSeverity(diag) >= CXDiagnostic_Error)
        {
            hadError = true;

            CXString diagString = clang_formatDiagnostic(diag,
                                                         clang_defaultDiagnosticDisplayOptions());

            fprintf(stderr, "%s\n", clang_getCString(diagString));

            clang_disposeString(diagString);
        }

        clang_disposeDiagnostic(diag);
    }

    /* Begin output generation. */
    mustache::basic_mustache<std::string> codeTemplate(gObjectGenTemplate);
    mustache::data codeData;

    /* The ignore errors flag exists because in the case of a compilation error
     * during the real build, we want the error to be reported by the actual
     * compiler because those errors are usually more informative and with nicer
     * formatting, etc. When this flag is set, we generate an output file that
     * only includes the offending source file, and return success so that the
     * build will proceed and error when the compiler tries to compile our
     * output. Note this only applies to clang errors, we still fail for our
     * own errors. */
    if (hadError)
    {
        if (!ignoreErrors)
        {
            return EXIT_FAILURE;
        }

        fprintf(stderr, "%s: warning: Failed to generate, continuing upon request\n", outputFile);
    }
    else
    {
        /* Iterate over the AST. */
        CXCursor cursor = clang_getTranslationUnitCursor(unit);
        ParsedTranslationUnit parsedUnit(cursor);
        ParsedDecl::VisitChildren(cursor, &parsedUnit);

        if (gParseErrorOccurred)
        {
            return EXIT_FAILURE;
        }

        if (dump)
        {
            parsedUnit.Dump(0);
            return EXIT_SUCCESS;
        }

        /* Generate the output data. */
        codeData = parsedUnit.Generate();
    }

    if (!standalone)
    {
        /* For now resolve the source file path to an absolute path, and use
         * that as the include. It's not ideal as things will break if the
         * build tree is moved around, so if this becomes an issue in future
         * we could instead try to calculate a relative path between the output
         * directory and the source file. */
        Path fullPath;
        if (!Filesystem::GetFullPath(Path(sourceFile, Path::kUnnormalizedPlatform), fullPath))
        {
            fprintf(stderr, "%s: Failed to get absolute path of '%s'\n", argv[0], sourceFile);
            return EXIT_FAILURE;
        }

        codeData.set("include", fullPath.ToPlatform());
    }

    codeTemplate.render(codeData, outputStream);

    /* We have succeeded, don't delete on exit. */
    outputStream.close();

    return EXIT_SUCCESS;
}
