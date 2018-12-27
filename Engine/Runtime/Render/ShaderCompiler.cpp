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

#include "Render/ShaderManager.h"

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

bool ShaderCompiler::LoadSource(const Path&  inPath,
                                const Path&  inFrom,
                                const size_t inLineIndex,
                                const size_t inDepth,
                                std::string& outSource)
{
    if (inDepth >= kMaximumIncludeDepth)
    {
        LogError("%s: Exceeded maximum include depth", inFrom.GetCString());
        return false;
    }

    /* First try the same directory as the current source file. */
    Path directoryName;
    if (inFrom.GetString() != kBuiltInFileName)
    {
        directoryName = inFrom.GetDirectoryName();
    }

    Path filePath = directoryName / inPath;
    bool found    = Filesystem::Exists(filePath);

    if (!found)
    {
        /* Try the search paths. */
        const auto& searchPaths = ShaderManager::Get().GetSearchPaths();

        const auto searchPath = searchPaths.find(inPath.Subset(0, 1).GetString());
        if (searchPath != searchPaths.end())
        {
            filePath = Path(searchPath->second) / inPath.Subset(1);
            found    = Filesystem::Exists(filePath);
        }
    }

    if (!found)
    {
        LogError("%s:%zu: Source file '%s' could not be found",
                 inFrom.GetCString(),
                 inLineIndex,
                 inPath.GetCString());

        return false;
    }

    std::unique_ptr<File> file(Filesystem::OpenFile(filePath));
    if (!file)
    {
        LogError("%s:%zu: Failed to open source file '%s'",
                 inFrom.GetCString(),
                 inLineIndex,
                 filePath.GetCString());

        return false;
    }

    outSource = StringUtils::Format("#line 1 \"%s\"\n", filePath.GetCString());

    const size_t headerLength = outSource.length();
    const size_t sourceLength = file->GetSize();

    outSource.resize(headerLength + sourceLength);
    if (!file->Read(outSource.data() + headerLength, sourceLength))
    {
        LogError("%s:%zu: Failed to read source file '%s'",
                 inFrom.GetCString(),
                 inLineIndex,
                 filePath.GetCString());

        return false;
    }

    outSource += StringUtils::Format("#line %zu \"%s\"", inLineIndex, inFrom.GetCString());

    mSourceFiles.emplace_back(filePath);

    return Preprocess(outSource, filePath, inDepth + 1);
}

bool ShaderCompiler::Preprocess(std::string& ioSource,
                                const Path&  inPath,
                                const size_t inDepth)
{
    /*
     * We have our own limited preprocessor in front of glslang, which handles
     * include directives. Although glslang's preprocessor does support
     * #include, we do not use this. The main reason is that the included source
     * is not substituted into the generated SPIR-V modules' OpSource when debug
     * info is enabled, meaning that it is not possible to directly edit shader
     * source in RenderDoc (we would need to set that up with the necessary
     * include paths on every use). Instead, we handle includes ourselves, and
     * substitute their content into the source passed to glslang.
     */

    size_t position  = 0;
    size_t lineIndex = 1;

    while (position < ioSource.size())
    {
        size_t newLine = ioSource.find('\n', position);
        if (newLine == std::string::npos)
        {
            newLine = ioSource.size();
        }

        if (ioSource[position] == '#')
        {
            const std::string line = ioSource.substr(position, newLine - position);

            std::vector<std::string> tokens;
            StringUtils::Tokenize(line, tokens, " \t", 2, true);

            if (tokens[0] == "#include")
            {
                if (tokens[1].length() < 2 ||
                    tokens[1][0] != '"' ||
                    tokens[1][tokens[1].length() - 1] != '"')
                {
                    LogError("%s:%zu: Malformed #include directive",
                             inPath.GetCString(),
                             lineIndex);

                    return false;
                }

                const Path includePath = Path(tokens[1].substr(1, tokens[1].length() - 2));

                std::string includeSource;
                if (!LoadSource(includePath,
                                inPath,
                                lineIndex,
                                inDepth,
                                includeSource))
                {
                    return false;
                }

                ioSource.replace(position, newLine - position, includeSource);

                newLine = position + includeSource.length();
            }
        }

        /* Move on to the next line. */
        position = newLine + 1;
        lineIndex++;
    }

    return true;
}

bool ShaderCompiler::GenerateSource(std::string& outSource)
{
    outSource = StringUtils::Format("#version %u core\n", kTargetGLSLVersion);
    outSource += "#extension GL_GOOGLE_cpp_style_line_directive : enable\n";
    outSource += "\n";

    if (!mOptions.source.empty())
    {
        /* Source string was provided, use that directly. */
        outSource += StringUtils::Format("#line 1 \"%s\"\n", kSourceStringName);
        outSource += mOptions.source;
    }
    else
    {
        /* Include the real source file, reusing the include logic to do so. */
        outSource += StringUtils::Format("#include \"%s\"\n", mOptions.path.GetCString());
    }

    return Preprocess(outSource, kBuiltInFileName, 0);
}

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
    std::string source;
    if (!GenerateSource(source))
    {
        return;
    }

    /* Initialise once on first use. */
    static const bool initialised = glslang::InitializeProcess();
    Assert(initialised);
    Unused(initialised);

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

    const bool parsed = shader.parse(&glslang::DefaultTBuiltInResource,
                                     kTargetGLSLVersion,
                                     ENoProfile,
                                     false,
                                     false,
                                     messages);

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
