/*
 * Copyright (C) 2018 Alex Smith
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

#include "Render/ShaderCompiler.h"

#include "Core/Filesystem.h"
#include "Core/String.h"

#include "glslang.h"

#include <memory>
#include <sstream>

static constexpr uint16_t kTargetGLSLVersion = 450;

static constexpr auto kBuiltInFileName  = "<built-in>";
static constexpr auto kSourceStringName = "<string>";

static constexpr size_t kMaximumIncludeDepth = 16;

namespace glslang
{
    extern const TBuiltInResource DefaultTBuiltInResource;
}

ShaderCompiler::ShaderCompiler(Options inOptions) :
    mOptions (std::move(inOptions))
{
}

std::string ShaderCompiler::GenerateSource()
{
    std::string source = StringUtils::Format("#version %u core\n", kTargetGLSLVersion);
    source += "#extension GL_GOOGLE_include_directive : enable\n";
    source += "#extension GL_GOOGLE_cpp_style_line_directive : enable\n";

    source += "\n";

    if (!mOptions.source.empty())
    {
        /* Source string was provided, use that directly. */
        source += StringUtils::Format("#line 1 \"%s\"\n", kSourceStringName);
        source += mOptions.source;
    }
    else
    {
        /* Include the real source file, reusing the include logic to do so. */
        source += StringUtils::Format("#include \"%s\"\n", mOptions.path.GetCString());
    }

    return source;
}

class ShaderCompiler::SourceIncluder : public glslang::TShader::Includer
{
public:
    /** Include result which stores its data in a std::string. */
    struct StringResult : IncludeResult
    {
        std::string data;

        StringResult(const std::string& inName,
                     std::string        inData) :
            IncludeResult (inName, nullptr, inData.length(), nullptr),
            data          (std::move(inData))
        {
            /* Bleh. Our data has to be initialised after we call base ctor. */
            const_cast<const char*&>(this->headerData) = this->data.c_str();
        }
    };

public:
    SourceIncluder(ShaderCompiler& inCompiler) :
        mCompiler (inCompiler)
    {
    }

    IncludeResult* includeLocal(const char*  inPath,
                                const char*  inFrom,
                                const size_t inDepth) override
    {
        if (inDepth >= kMaximumIncludeDepth)
        {
            return MakeErrorResult("Exceeded maximum include depth");
        }

        Path directoryName;
        if (std::strcmp(inFrom, kBuiltInFileName))
        {
            directoryName = Path(inFrom).GetDirectoryName();
        }

        Path fileName = directoryName / inPath;

        std::unique_ptr<File> file(Filesystem::OpenFile(fileName));
        if (!file)
        {
            return MakeErrorResult(StringUtils::Format("Failed to open '%s'", inPath));
        }

        std::string data;
        data.resize(file->GetSize());
        if (!file->Read(data.data(), data.size()))
        {
            return MakeErrorResult(StringUtils::Format("Failed to read '%s'", inPath));
        }

        mCompiler.mSourceFiles.emplace_back(fileName);

        return new StringResult(fileName.GetString(), std::move(data));
    }

    void releaseInclude(IncludeResult* inResult) override
    {
        delete static_cast<StringResult*>(inResult);
    }
private:
    static IncludeResult* MakeErrorResult(std::string inMessage)
    {
        return new StringResult(std::string(""), std::move(inMessage));
    }

private:
    ShaderCompiler&             mCompiler;

};

static void LogGLSLMessages(const char* const inLog)
{
    std::istringstream stream(inLog);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.empty())
        {
            continue;
        }

        static constexpr auto errorPrefix   = "ERROR: ";
        static constexpr auto warningPrefix = "WARNING: ";

        if (!line.compare(0, strlen(errorPrefix), errorPrefix))
        {
            std::string message = line.substr(strlen(errorPrefix));
            LogError("%s", message.c_str());
        }
        else if (!line.compare(0, strlen(warningPrefix), warningPrefix))
        {
            std::string message = line.substr(strlen(warningPrefix));
            LogWarning("%s", message.c_str());
        }
    }
}

static bool LogSPIRVMessages(const char* const          inPath,
                             const spv::SpvBuildLogger& inLogger)
{
    std::istringstream stream(inLogger.getAllMessages());
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.empty())
        {
            continue;
        }

        static constexpr auto warningPrefix = "warning: ";

        const LogLevel level = (!line.compare(0, strlen(warningPrefix), warningPrefix))
                                   ? kLogLevel_Warning
                                   : kLogLevel_Error;

        LogMessage(level, "%s: SPIR-V %s", inPath, line.c_str());

        if (level == kLogLevel_Error)
        {
            return false;
        }
    }

    return true;
}

void ShaderCompiler::Compile()
{
    Assert(!IsCompiled());

    const char* const sourcePath = (!mOptions.source.empty())
                                       ? kSourceStringName
                                       : mOptions.path.GetCString();

    /* Generate the source string to pass to the compiler, containing built in
     * definitions. This #includes the real source file, so the logic for
     * loading that is in the includer. */
    std::string source = GenerateSource();

    /* Initialise once on first use. */
    static const bool initialised = glslang::InitializeProcess();
    Assert(initialised);

    /* Convert stage to a glslang type. */
    EShLanguage glslangStage;
    switch (mOptions.stage)
    {
        case kGPUShaderStage_Vertex:    glslangStage = EShLangVertex; break;
        case kGPUShaderStage_Fragment:  glslangStage = EShLangFragment; break;
        case kGPUShaderStage_Compute:   glslangStage = EShLangCompute; break;

        default:                        Unreachable();
    }

    EShMessages messages = static_cast<EShMessages>(EShMsgVulkanRules | EShMsgSpvRules);

    #if ORION_BUILD_DEBUG
        messages = static_cast<EShMessages>(messages | EShMsgDebugInfo);
    #endif

    /* Parse the shader. */
    const char* const sourceString = source.c_str();
    const int sourceLength         = source.length();
    const char* const sourceName   = kBuiltInFileName;

    glslang::TShader shader(glslangStage);
    shader.setStringsWithLengthsAndNames(&sourceString, &sourceLength, &sourceName, 1);

    SourceIncluder includer(*this);
    const bool parsed = shader.parse(&glslang::DefaultTBuiltInResource,
                                     kTargetGLSLVersion,
                                     ENoProfile,
                                     false,
                                     false,
                                     messages,
                                     includer);

    LogGLSLMessages(shader.getInfoLog());

    if (!parsed)
    {
        return;
    }

    /* Link the shader. */
    glslang::TProgram program;
    program.addShader(&shader);

    const bool linked = program.link(messages);

    LogGLSLMessages(program.getInfoLog());

    if (!linked)
    {
        return;
    }

    /* Generate SPIR-V. */
    glslang::TIntermediate* intermediate = program.getIntermediate(glslangStage);
    Assert(intermediate);

    glslang::SpvOptions options;
    #if ORION_BUILD_DEBUG
        options.generateDebugInfo = true;
    #endif

    spv::SpvBuildLogger logger;
    glslang::GlslangToSpv(*intermediate, mCode, &logger, &options);

    if (!LogSPIRVMessages(sourcePath, logger))
    {
        mCode.clear();
    }
}

bool ShaderCompiler::CompileFile(Path                 inPath,
                                 const GPUShaderStage inStage,
                                 GPUShaderCode&       outCode)
{
    ShaderCompiler::Options options;
    options.path  = std::move(inPath);
    options.stage = inStage;

    ShaderCompiler compiler(options);
    compiler.Compile();

    if (compiler.IsCompiled())
    {
        outCode = compiler.MoveCode();
        return true;
    }
    else
    {
        return false;
    }
}

bool ShaderCompiler::CompileString(std::string          inSource,
                                   const GPUShaderStage inStage,
                                   GPUShaderCode&       outCode)
{
    ShaderCompiler::Options options;
    options.source = std::move(inSource);
    options.stage  = inStage;

    ShaderCompiler compiler(options);
    compiler.Compile();

    if (compiler.IsCompiled())
    {
        outCode = compiler.MoveCode();
        return true;
    }
    else
    {
        return false;
    }
}
