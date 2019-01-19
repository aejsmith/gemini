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

#ifdef ORION_PLATFORM_WIN32
    #include <windows.h>
    #include <tchar.h>
#endif

#include "ObjectGen.mustache.h"

using namespace kainjow;

class ParsedTranslationUnit;

class ParsedDecl
{
public:
    using VisitFunction           = std::function<void (CXCursor, CXCursorKind)>;

public:
                                    ParsedDecl(CXCursor inCursor,
                                               ParsedDecl* const inParent        = nullptr,
                                               const bool        inNameFromType  = false);

    virtual                         ~ParsedDecl() {}

public:
    bool                            IsFromMainFile() const;
    ParsedTranslationUnit*          GetTranslationUnit();

    virtual mustache::data          Generate() const
                                        { return mustache::data(); }

    virtual void                    Dump(const unsigned inDepth) const = 0;

    static void                     VisitChildren(CXCursor          inCursor,
                                                  ParsedDecl* const inDecl);

    static void                     VisitChildren(CXCursor             inCursor,
                                                  const VisitFunction& inFunction);

public:
    CXCursor                        cursor;
    ParsedDecl*                     parent;
    std::string                     name;
    bool                            isAnnotated;

protected:
    virtual bool                    HandleAnnotation(const std::string&         inType,
                                                     const rapidjson::Document& inAttributes)
                                        { return false; }

    virtual void                    HandleChild(CXCursor     inCursor,
                                                CXCursorKind inKind) {}

private:
    static CXChildVisitResult       VisitDeclCallback(CXCursor     inCursor,
                                                      CXCursor     inParent,
                                                      CXClientData inData);

    static CXChildVisitResult       VisitFunctionCallback(CXCursor     inCursor,
                                                          CXCursor     inParent,
                                                          CXClientData inData);

};

class ParsedProperty : public ParsedDecl
{
public:
                                    ParsedProperty(CXCursor          inCursor,
                                                   ParsedDecl* const inParent);

public:
    mustache::data                  Generate() const override;
    void                            Dump(const unsigned inDepth) const override;

public:
    std::string                     type;
    std::string                     getFunction;
    std::string                     setFunction;

    /** Behaviour flags. */
    bool                            transient;

protected:
    bool                            HandleAnnotation(const std::string&         inType,
                                                     const rapidjson::Document& inAttributes) override;

};

using ParsedPropertyList = std::list<std::unique_ptr<ParsedProperty>>;

class ParsedClass : public ParsedDecl
{
public:
                                    ParsedClass(CXCursor          inCursor,
                                                ParsedDecl* const inParent);

public:
    bool                            IsObject() const;
    bool                            IsConstructable() const;
    bool                            IsPublicConstructable() const;

    mustache::data                  Generate() const override;
    void                            Dump(const unsigned inDepth) const override;

public:
    bool                            isObjectDerived;
    ParsedClass*                    parentClass;

    ParsedPropertyList              properties;

protected:
    bool                            HandleAnnotation(const std::string&         inType,
                                                     const rapidjson::Document& inAttributes) override;

    void                            HandleChild(CXCursor     inCursor,
                                                CXCursorKind inKind) override;

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
    Constructability                mConstructable;

    /** Temporary state used while parsing. */
    bool                            mOnMetaClass;

};

using ParsedClassMap = std::map<std::string, std::unique_ptr<ParsedClass>>;

class ParsedEnum : public ParsedDecl
{
public:
    mustache::data                  Generate() const override;
    void                            Dump(const unsigned inDepth) const override;

    static void                     Create(CXCursor          inCursor,
                                           ParsedDecl* const inParent);

public:
    /** Whether this enum is used and should have code generated. */
    bool                            shouldGenerate;

protected:
    bool                            HandleAnnotation(const std::string&         inType,
                                                     const rapidjson::Document& inAttributes) override;

    void                            HandleChild(CXCursor     inCursor,
                                                CXCursorKind inKind) override;

private:
    using EnumConstant            = std::pair<std::string, long long>;
    using EnumConstantList        = std::list<EnumConstant>;

private:
                                    ParsedEnum(CXCursor    inCursor,
                                               ParsedDecl* inParent);

private:
    EnumConstantList                mConstants;

};

using ParsedEnumMap = std::map<std::string, std::unique_ptr<ParsedEnum>>;

class ParsedTranslationUnit : public ParsedDecl
{
public:
    explicit                        ParsedTranslationUnit(CXCursor inCursor);

public:
    mustache::data                  Generate() const override;
    void                            Dump(const unsigned inDepth) const override;

public:
    /** List of child classes. */
    ParsedClassMap                  classes;

    /** List of child enumerations (including ones nested within classes). */
    ParsedEnumMap                   enums;

protected:
    void                            HandleChild(CXCursor     inCursor,
                                                CXCursorKind inKind) override;

};

static bool gParseErrorOccurred = false;

static void ParseError(CXCursor          inCursor,
                       const char* const inFormat,
                       ...)
{
    CXSourceLocation location = clang_getCursorLocation(inCursor);
    CXFile file;
    unsigned line, column;
    clang_getSpellingLocation(location, &file, &line, &column, nullptr);
    CXString fileName = clang_getFileName(file);

    fprintf(stderr, "%s:%u:%u: error: ", clang_getCString(fileName), line, column);

    va_list args;
    va_start(args, inFormat);
    vfprintf(stderr, inFormat, args);
    va_end(args);

    fprintf(stderr, "\n");

    gParseErrorOccurred = true;
}

/**
 * In the generated code we base the name of some of the variables we define
 * on the name of the class or enum. For a class or enum that is nested in a
 * namespace or inside another class, the name is of the form "Foo::Bar". This
 * cannot be directly used to name a variable, e.g. "Foo::Bar_data". This
 * function solves this by replacing "::" in the name string with "_" to give
 * a name suitable for naming our generated variables.
 */
static std::string MangleName(const std::string& inName)
{
    std::string mangled(inName);

    size_t pos;
    while ((pos = mangled.find("::")) != std::string::npos)
    {
        mangled.replace(pos, 2, "_");
    }

    return mangled;
}

ParsedDecl::ParsedDecl(CXCursor          inCursor,
                       ParsedDecl* const inParent,
                       const bool        inNameFromType) :
    cursor      (inCursor),
    parent      (inParent),
    isAnnotated (false)
{
    CXString name;

    if (inNameFromType)
    {
        CXType type = clang_getCursorType(this->cursor);
        name = clang_getTypeSpelling(type);
    }
    else
    {
        name = clang_getCursorSpelling(cursor);
    }

    this->name = clang_getCString(name);
    clang_disposeString(name);
}

bool ParsedDecl::IsFromMainFile() const
{
    return clang_Location_isFromMainFile(clang_getCursorLocation(this->cursor));
}

ParsedTranslationUnit* ParsedDecl::GetTranslationUnit()
{
    ParsedDecl* decl = this;

    while (decl->parent)
    {
        decl = decl->parent;
    }

    return static_cast<ParsedTranslationUnit*>(decl);
}

static bool ParseAnnotation(CXCursor             inCursor,
                            std::string&         outType,
                            rapidjson::Document& outAttributes)
{
    CXString str = clang_getCursorSpelling(inCursor);
    std::string annotation(clang_getCString(str));
    clang_disposeString(str);

    std::vector<std::string> tokens;
    StringUtils::Tokenize(annotation, tokens, ":", 3, false);

    if (tokens[0].compare("orion"))
    {
        /* Don't raise an error for annotations that aren't marked as being for
         * us, could be annotations for other reasons. */
        return false;
    }
    else if (tokens.size() != 3)
    {
        ParseError(inCursor, "malformed annotation");
        return false;
    }

    outType = tokens[1];

    std::string json = std::string("{") + tokens[2] + std::string("}");

    outAttributes.Parse(json.c_str());

    if (outAttributes.HasParseError())
    {
        ParseError(inCursor,
                   "parse error in attributes (at %zu): %s",
                   outAttributes.GetErrorOffset() - 1,
                   rapidjson::GetParseError_En(outAttributes.GetParseError()));

        return false;
    }

    return true;
}

CXChildVisitResult ParsedDecl::VisitDeclCallback(CXCursor     inCursor,
                                                 CXCursor     inParent,
                                                 CXClientData inData)
{
    auto currentDecl = reinterpret_cast<ParsedDecl*>(inData);

    CXCursorKind kind = clang_getCursorKind(inCursor);
    if (kind == CXCursor_AnnotateAttr)
    {
        std::string type;
        rapidjson::Document attributes;
        if (!ParseAnnotation(inCursor, type, attributes))
        {
            return CXChildVisit_Continue;
        }

        if (currentDecl->HandleAnnotation(type, attributes))
        {
            currentDecl->isAnnotated = true;
        }
        else
        {
            ParseError(inCursor, "unexpected '%s' annotation", type.c_str());
        }
    }
    else
    {
        currentDecl->HandleChild(inCursor, kind);
    }

    return CXChildVisit_Continue;
}

void ParsedDecl::VisitChildren(CXCursor    inCursor,
                               ParsedDecl* inDecl)
{
    clang_visitChildren(inCursor, VisitDeclCallback, inDecl);
}

CXChildVisitResult ParsedDecl::VisitFunctionCallback(CXCursor     inCursor,
                                                     CXCursor     inParent,
                                                     CXClientData inData)
{
    auto& function = *reinterpret_cast<const VisitFunction*>(inData);

    CXCursorKind kind = clang_getCursorKind(inCursor);
    function(inCursor, kind);

    return CXChildVisit_Continue;
}

void ParsedDecl::VisitChildren(CXCursor             inCursor,
                               const VisitFunction& inFunction)
{
    clang_visitChildren(inCursor,
                        VisitFunctionCallback,
                        const_cast<VisitFunction*>(&inFunction));
}

ParsedProperty::ParsedProperty(CXCursor          inCursor,
                               ParsedDecl* const inParent) :
    ParsedDecl (inCursor, inParent),
    transient  (false)
{
    /* Remove prefixes from virtual property names. */
    if (this->name.substr(0, 6) == "vprop_")
    {
        this->name = this->name.substr(6);
    }

    /* Get the property type. */
    CXType type  = clang_getCursorType(inCursor);
    CXString str = clang_getTypeSpelling(type);
    this->type   = clang_getCString(str);
    clang_disposeString(str);
}

bool ParsedProperty::HandleAnnotation(const std::string&         inType,
                                      const rapidjson::Document& inAttributes)
{
    if (inType.compare("property") != 0)
    {
        return false;
    }

    /* Now that we know that we are really a property, if our type is an enum,
     * mark that enum for code generation. */
    CXType propertyType       = clang_getCursorType(this->cursor);
    CXCursor propertyTypeDecl = clang_getTypeDeclaration(propertyType);

    if (clang_getCursorKind(propertyTypeDecl) == CXCursor_EnumDecl)
    {
        const ParsedTranslationUnit* translationUnit = GetTranslationUnit();

        auto it = translationUnit->enums.find(this->type);
        if (it != translationUnit->enums.end())
        {
            it->second->shouldGenerate = true;
        }
        else
        {
            ParseError(this->cursor,
                       "full declaration of enum '%s' must be available for property '%s'",
                       this->type.c_str(),
                       this->name.c_str());

            return true;
        }
    }

    auto parent = static_cast<ParsedClass*>(this->parent);

    if (!parent->isObjectDerived)
    {
        ParseError(this->cursor,
                   "'property' annotation on field '%s' in non-Object class '%s'",
                   this->name.c_str(),
                   this->parent->name.c_str());

        return true;
    }

    static const char* kGetAttribute       = "get";
    static const char* kSetAttribute       = "set";
    static const char* kTransientAttribute = "transient";

    if (inAttributes.HasMember(kGetAttribute))
    {
        const rapidjson::Value& value = inAttributes[kGetAttribute];

        if (!value.IsString())
        {
            ParseError(this->cursor, "'%s' attribute must be a string", kGetAttribute);
            return true;
        }

        this->getFunction = value.GetString();
    }

    if (inAttributes.HasMember(kSetAttribute))
    {
        const rapidjson::Value& value = inAttributes[kSetAttribute];

        if (!value.IsString())
        {
            ParseError(this->cursor, "'%s' attribute must be a string", kSetAttribute);
            return true;
        }

        this->setFunction = value.GetString();
    }

    if (this->getFunction.empty() != this->setFunction.empty())
    {
        ParseError(this->cursor, "both 'get' and 'set' or neither of them must be specified");
        return true;
    }

    if (inAttributes.HasMember(kTransientAttribute))
    {
        const rapidjson::Value& value = inAttributes[kTransientAttribute];

        if (!value.IsBool())
        {
            ParseError(this->cursor, "'%s' attribute must be a boolean", kTransientAttribute);
            return true;
        }

        this->transient = value.GetBool();
    }

    if (clang_getCXXAccessSpecifier(this->cursor) != CX_CXXPublic)
    {
        ParseError(this->cursor, "property '%s' must be public", this->name.c_str());
        return true;
    }

    const bool isVirtual = clang_getCursorKind(this->cursor) == CXCursor_VarDecl;

    if (isVirtual)
    {
        if (this->getFunction.empty())
        {
            /* These require getters and setters. If they are omitted, default
             * names are used based on the property name (see Object.h). */
            std::string name = static_cast<char>(toupper(this->name[0])) + this->name.substr(1);

            this->getFunction = std::string("Get") + name;
            this->setFunction = std::string("Set") + name;
        }
    }
    else if (!this->getFunction.empty())
    {
        /* This makes no sense - code can directly access/modify the property
         * so usage of getter/setter methods should not be required. */
        ParseError(this->cursor, "public properties cannot have getter/setter methods");
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
        [&] (const char* inFlag)
        {
            if (!flags.empty())
            {
                flags += " | ";
            }

            flags += "MetaProperty::";
            flags += inFlag;
        };

    if (this->transient)
        AddFlag("kTransient");

    if (flags.empty())
        flags = "0";

    data.set("propertyName", this->name);
    data.set("propertyType", this->type);
    data.set("propertyFlags", flags);

    if (!this->getFunction.empty())
    {
        data.set("propertyGet", this->getFunction);
        data.set("propertySet", this->setFunction);
    }

    return data;
}

void ParsedProperty::Dump(const unsigned inDepth) const
{
    printf("%-*sProperty '%s' (type '%s', get '%s', set '%s')\n",
           inDepth * 2, "",
           this->name.c_str(),
           this->type.c_str(),
           this->getFunction.c_str(),
           this->setFunction.c_str());
}

ParsedClass::ParsedClass(CXCursor          inCursor,
                         ParsedDecl* const inParent) :
    ParsedDecl      (inCursor, inParent, true),
    isObjectDerived (name.compare("Object") == 0),
    parentClass     (nullptr),
    mConstructable  (kConstructability_Default),
    mOnMetaClass    (false)
{
}

bool ParsedClass::IsObject() const
{
    if (this->isAnnotated && this->isObjectDerived)
    {
        return true;
    }
    else if (this->isObjectDerived)
    {
        ParseError(this->cursor,
                   "Object-derived class '%s' missing 'class' annotation; CLASS() macro missing?",
                   this->name.c_str());
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

bool ParsedClass::HandleAnnotation(const std::string&         inType,
                                   const rapidjson::Document& inAttributes)
{
    if (!mOnMetaClass || inType.compare("class") != 0)
    {
        return false;
    }

    if (!this->isObjectDerived)
    {
        ParseError(cursor,
                   "'class' annotation on non-Object class '%s'",
                   this->name.c_str());
    }

    if (inAttributes.HasMember("constructable"))
    {
        const rapidjson::Value& value = inAttributes["constructable"];

        if (!value.IsBool())
        {
            ParseError(this->cursor, "'constructable' attribute must be a boolean");
            return true;
        }

        const bool constructable = value.GetBool();
        if (constructable)
        {
            ParseError(this->cursor, "constructability cannot be forced on, only off");
            return true;
        }

        mConstructable = kConstructability_ForcedNone;
    }

    return true;
}

void ParsedClass::HandleChild(CXCursor     inCursor,
                              CXCursorKind inKind)
{
    if (mOnMetaClass)
    {
        return;
    }

    switch (inKind)
    {
        case CXCursor_CXXBaseSpecifier:
        {
            /* Check if this class is derived from Object. This gives us the
             * fully-qualified name (with all namespaces) regardless of whether
             * it was specified that way in the source. */
            CXType type  = clang_getCursorType(inCursor);
            CXString str = clang_getTypeSpelling(type);
            std::string typeName(clang_getCString(str));
            clang_disposeString(str);

            /* The translation unit records all Object-derived classes seen,
             * even those outside the main file. Therefore, we look for the
             * base class name in there, and if it matches one of those, then
             * we are an Object-derived class as well. */
            const ParsedTranslationUnit* translationUnit = GetTranslationUnit();

            auto it = translationUnit->classes.find(typeName);
            if (it != translationUnit->classes.end())
            {
                /* If isObjectDerived is already set to true, then we have
                 * multiple inheritance, which is unsupported. */
                if (this->isObjectDerived)
                {
                    ParseError(inCursor,
                               "Inheritance from multiple Object-derived classes is unsupported (on class '%s')",
                               this->name.c_str());
                }

                this->isObjectDerived = true;
                this->parentClass     = it->second.get();
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
                inCursor,
                [&numParams] (CXCursor, CXCursorKind inKind)
                {
                    if (inKind == CXCursor_ParmDecl)
                    {
                        numParams++;
                    }
                });

            /* Only constructors with no parameters are suitable. */
            if (numParams == 0)
            {
                if (clang_getCXXAccessSpecifier(inCursor) == CX_CXXPublic)
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

        case CXCursor_VarDecl:
        {
            /* Static class variables fall under VarDecl. The class annotation
             * is applied to the staticMetaClass member, so if we have that
             * variable, then descend onto children keeping the same current
             * declaration so we see the annotation below. */
            CXString str = clang_getCursorSpelling(inCursor);
            std::string typeName(clang_getCString(str));
            clang_disposeString(str);

            if (!typeName.compare("staticMetaClass"))
            {
                mOnMetaClass = true;
                VisitChildren(inCursor, this);
                mOnMetaClass = false;
                break;
            }

            /* Fall through to check for virtual properties. */
        }

        case CXCursor_FieldDecl:
        {
            /* FieldDecl is an instance variable. Look for properties. */
            std::unique_ptr<ParsedProperty> parsedProperty(new ParsedProperty(inCursor, this));
            VisitChildren(inCursor, parsedProperty.get());

            if (parsedProperty->isAnnotated)
            {
                this->properties.emplace_back(std::move(parsedProperty));
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
            if (clang_CXXMethod_isPureVirtual(inCursor))
            {
                mConstructable = kConstructability_ForcedNone;
            }

            break;
        }

        case CXCursor_EnumDecl:
        {
            ParsedEnum::Create(inCursor, this);
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

    data.set("name", this->name);
    data.set("mangledName", MangleName(this->name));

    if (this->parentClass)
    {
        data.set("parent", this->parentClass->name);
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
    for (const std::unique_ptr<ParsedProperty> &parsedProperty : this->properties)
    {
        properties.push_back(parsedProperty->Generate());
    }

    data.set("properties", properties);

    return data;
}

void ParsedClass::Dump(const unsigned inDepth) const
{
    printf("%-*sClass '%s' (", inDepth * 2, "", this->name.c_str());

    if (this->parentClass)
    {
        printf("parent '%s', ", this->parentClass->name.c_str());
    }

    printf("constructable %d %d)\n", IsConstructable(), IsPublicConstructable());

    for (const std::unique_ptr<ParsedProperty> &parsedProperty : this->properties)
    {
        parsedProperty->Dump(inDepth + 1);
    }
}

ParsedEnum::ParsedEnum(CXCursor          inCursor,
                       ParsedDecl* const inParent) :
    ParsedDecl     (inCursor, inParent, true),
    shouldGenerate (false)
{
}

void ParsedEnum::Create(CXCursor          inCursor,
                        ParsedDecl* const inParent)
{
    if (!clang_isCursorDefinition(inCursor))
    {
        return;
    }

    /* We don't handle anonymous enums. There is no function that specifically
     * identifies this, so the way we do this is to check if the cursor
     * spelling is empty. Have to do this separately rather than checking the
     * name obtained by the constructor because that gets the type spelling
     * which is not empty for an anonymous enum. */
    CXString name        = clang_getCursorSpelling(inCursor);
    const bool anonymous = std::strlen(clang_getCString(name)) == 0;
    clang_disposeString(name);
    if (anonymous)
    {
        return;
    }

    std::unique_ptr<ParsedEnum> parsedEnum(new ParsedEnum(inCursor, inParent));

    VisitChildren(inCursor, parsedEnum.get());

    inParent->GetTranslationUnit()->enums.insert(std::make_pair(parsedEnum->name, std::move(parsedEnum)));
}

bool ParsedEnum::HandleAnnotation(const std::string&         inType,
                                  const rapidjson::Document& inAttributes)
{
    if (inType.compare("enum") != 0)
    {
        return false;
    }

    this->shouldGenerate = true;
    return true;
}

void ParsedEnum::HandleChild(CXCursor     inCursor,
                             CXCursorKind inKind)
{
    if (inKind == CXCursor_EnumConstantDecl)
    {
        long long value = clang_getEnumConstantDeclValue(cursor);

        CXString str = clang_getCursorSpelling(cursor);
        std::string name(clang_getCString(str));
        clang_disposeString(str);

        mConstants.push_back(std::make_pair(name, value));
    }
}

mustache::data ParsedEnum::Generate() const
{
    mustache::data data;

    data.set("name", this->name);
    data.set("mangledName", MangleName(this->name));

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

void ParsedEnum::Dump(const unsigned inDepth) const
{
    printf("%-*sEnum '%s'\n", inDepth * 2, "", this->name.c_str());

    for (auto& pair : mConstants)
    {
        printf("%-*s'%s' = %lld\n", (inDepth + 1) * 2, "", pair.first.c_str(), pair.second);
    }
}

ParsedTranslationUnit::ParsedTranslationUnit(CXCursor inCursor) :
    ParsedDecl (inCursor)
{
}

void ParsedTranslationUnit::HandleChild(CXCursor     inCursor,
                                        CXCursorKind inKind)
{
    switch (inKind)
    {
        case CXCursor_Namespace:
            /* Descend into namespaces. */
            VisitChildren(inCursor, this);
            break;

        case CXCursor_ClassDecl:
        case CXCursor_StructDecl:
            /* Ignore forward declarations. */
            if (clang_isCursorDefinition(inCursor))
            {
                std::unique_ptr<ParsedClass> parsedClass(new ParsedClass(inCursor, this));
                VisitChildren(inCursor, parsedClass.get());

                if (parsedClass->IsObject())
                {
                    this->classes.insert(std::make_pair(parsedClass->name, std::move(parsedClass)));
                }
            }

            break;

        case CXCursor_EnumDecl:
            ParsedEnum::Create(inCursor, this);
            break;

        default:
            break;

    }
}

mustache::data ParsedTranslationUnit::Generate() const
{
    mustache::data classes(mustache::data::type::list);
    for (auto& it : this->classes)
    {
        const std::unique_ptr<ParsedClass>& parsedClass = it.second;

        if (parsedClass->IsFromMainFile())
        {
            classes.push_back(parsedClass->Generate());
        }
    }

    mustache::data enums(mustache::data::type::list);
    for (auto& it : this->enums)
    {
        const std::unique_ptr<ParsedEnum>& parsedEnum = it.second;

        if (parsedEnum->shouldGenerate)
        {
            enums.push_back(parsedEnum->Generate());
        }
    }

    mustache::data data;
    data.set("classes", classes);
    data.set("enums", enums);
    return data;
}

void ParsedTranslationUnit::Dump(const unsigned inDepth) const
{
    printf("%-*sTranslationUnit '%s'\n", inDepth * 2, "", this->name.c_str());

    for (auto& it : this->classes)
    {
        const std::unique_ptr<ParsedClass>& parsedClass = it.second;

        if (parsedClass->IsFromMainFile())
        {
            parsedClass->Dump(inDepth + 1);
        }
    }

    for (auto& it : this->enums)
    {
        const std::unique_ptr<ParsedEnum>& parsedEnum = it.second;

        if (parsedEnum->shouldGenerate)
        {
            parsedEnum->Dump(inDepth + 1);
        }
    }
}

static void Usage(const char* inProgramName)
{
    printf("Usage: %s [options...] <source> <output>\n", inProgramName);
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
        explicit OutputStream(const char* inFile) :
            std::ofstream (inFile, std::ofstream::out | std::ofstream::trunc),
            file          (inFile)
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
    clangArgs.push_back("-DORION_OBJGEN=1");
    #if ORION_PLATFORM_WIN32
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
