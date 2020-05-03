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

#include "Render/ShaderCompiler.h"

#include "Core/Filesystem.h"
#include "Core/String.h"

#include "Render/ShaderManager.h"
#include "Render/ShaderTechnique.h"

#include "shaderc/shaderc.hpp"

#include <memory>
#include <sstream>

static constexpr char kBuiltInFileName[]     = "<built-in>";
static constexpr size_t kMaximumIncludeDepth = 16;

ShaderCompiler::ShaderCompiler(Options options) :
    mOptions (std::move(options))
{
}

bool ShaderCompiler::LoadSource(const Path&  path,
                                const Path&  from,
                                const size_t lineIndex,
                                const size_t depth,
                                std::string& outSource)
{
    if (depth >= kMaximumIncludeDepth)
    {
        LogError("%s: Exceeded maximum include depth", from.GetCString());
        return false;
    }

    /* First try the same directory as the current source file. */
    Path directoryName;
    if (from.GetString() != kBuiltInFileName)
    {
        directoryName = from.GetDirectoryName();
    }

    Path filePath = directoryName / path;
    bool found    = Filesystem::Exists(filePath);

    if (!found)
    {
        /* Try the search paths. */
        const auto& searchPaths = ShaderManager::Get().GetSearchPaths();

        const auto searchPath = searchPaths.find(path.Subset(0, 1).GetString());
        if (searchPath != searchPaths.end())
        {
            filePath = Path(searchPath->second) / path.Subset(1);
            found    = Filesystem::Exists(filePath);
        }
    }

    if (!found)
    {
        LogError("%s:%zu: Source file '%s' could not be found",
                 from.GetCString(),
                 lineIndex,
                 path.GetCString());

        return false;
    }

    UPtr<File> file(Filesystem::OpenFile(filePath));
    if (!file)
    {
        LogError("%s:%zu: Failed to open source file '%s'",
                 from.GetCString(),
                 lineIndex,
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
                 from.GetCString(),
                 lineIndex,
                 filePath.GetCString());

        return false;
    }

    outSource += StringUtils::Format("#line %zu \"%s\"", lineIndex, from.GetCString());

    mSourceFiles.emplace_back(filePath);

    return Preprocess(outSource, filePath, depth + 1);
}

bool ShaderCompiler::Preprocess(std::string& ioSource,
                                const Path&  path,
                                const size_t depth)
{
    /*
     * We have our own limited preprocessor in front of shaderc, which handles
     * include directives. Although shaderc's preprocessor does support
     * #include, we do not use this. The main reason is that the included source
     * is not substituted into the generated SPIR-V modules' OpSource when debug
     * info is enabled, meaning that it is not possible to directly edit shader
     * source in RenderDoc (we would need to set that up with the necessary
     * include paths on every use). Instead, we handle includes ourselves, and
     * substitute their content into the source passed to shaderc.
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
                             path.GetCString(),
                             lineIndex);

                    return false;
                }

                const Path includePath = Path(tokens[1].substr(1, tokens[1].length() - 2));

                std::string includeSource;
                if (!LoadSource(includePath,
                                path,
                                lineIndex,
                                depth,
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

bool ShaderCompiler::GenerateSource()
{
    mSource.clear();

    mSource += "#define __HLSL__ 1\n";

    if (mOptions.technique)
    {
        /* Generate technique parameter definitions. */
        const ShaderTechnique* const technique = mOptions.technique;

        std::string constantBuffer;

        for (const ShaderParameter& parameter : technique->GetParameters())
        {
            if (ShaderParameter::IsConstant(parameter.type))
            {
                if (constantBuffer.empty())
                {
                    constantBuffer += StringUtils::Format("cbuffer MaterialConstants : register(b%u, space%d)\n{\n",
                                                          technique->GetConstantsIndex(),
                                                          kArgumentSet_Material);
                }

                /* The parameter array includes constant parameters in order
                 * of offset in the constant buffer. Offsets have taken care of
                 * HLSL packing rules, so we can just declare in order of
                 * appearence here. */
                constantBuffer += StringUtils::Format("    %s %s;\n",
                                                      ShaderParameter::GetHLSLType(parameter.type),
                                                      parameter.name.c_str());
            }
            else
            {
                bool needsSampler = false;

                switch (parameter.type)
                {
                    case kShaderParameterType_Texture2D:
                        mSource += StringUtils::Format("Texture2D %s_texture : register(t%u, space%d);\n",
                                                       parameter.name.c_str(),
                                                       parameter.argumentIndex,
                                                       kArgumentSet_Material);

                        needsSampler = true;
                        break;

                    default:
                        UnreachableMsg("Unhandled ShaderParameterType");

                }

                if (needsSampler)
                {
                    /* Samplers are at argumentIndex + 1. */
                    mSource += StringUtils::Format("SamplerState %s_sampler : register(s%u, space%d);\n",
                                                   parameter.name.c_str(),
                                                   parameter.argumentIndex + 1,
                                                   kArgumentSet_Material);
                }
            }
        }

        if (!constantBuffer.empty())
        {
            constantBuffer += "};\n";
            mSource += constantBuffer;
        }
    }

    /* Include the real source file, reusing the include logic to do so. */
    mSource += StringUtils::Format("#include \"%s\"\n", mOptions.path.GetCString());

    return Preprocess(mSource, kBuiltInFileName, 0);
}

void ShaderCompiler::Compile()
{
    Assert(!IsCompiled());

    /* Generate the source string to pass to the compiler, containing built in
     * definitions. This #includes the real source file, so the logic for
     * loading that is in the includer. */
    if (!GenerateSource())
    {
        return;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetSourceLanguage(shaderc_source_language_hlsl);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
    options.SetHlslFunctionality1(true);

    #if GEMINI_BUILD_DEBUG
        options.SetGenerateDebugInfo();
    #endif

    shaderc_shader_kind shadercKind;
    switch (mOptions.stage)
    {
        case kGPUShaderStage_Vertex:    shadercKind = shaderc_vertex_shader; break;
        case kGPUShaderStage_Pixel:     shadercKind = shaderc_fragment_shader; break;
        case kGPUShaderStage_Compute:   shadercKind = shaderc_compute_shader; break;

        default:                        Unreachable();
    }

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(mSource.c_str(),
                                  shadercKind,
                                  kBuiltInFileName,
                                  mOptions.function.c_str(),
                                  options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        LogError("%s", module.GetErrorMessage().c_str());
        return;
    }
    else if (module.GetNumWarnings() > 0)
    {
        LogWarning("%s", module.GetErrorMessage().c_str());
    }

    mCode = {module.cbegin(), module.cend()};
}
