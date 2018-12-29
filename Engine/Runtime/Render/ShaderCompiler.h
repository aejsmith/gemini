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

#pragma once

#include "Core/Path.h"

#include "Render/RenderDefs.h"

#include <list>

/**
 * Class for compiling HLSL shaders to SPIR-V. Note that while we are currently
 * doing compilation entirely at runtime, it is intended in future that "baked"
 * game data would include only the pre-compiled target API shaders, and the
 * compiler would not be available in the final game/engine build.
 */
class ShaderCompiler
{
public:
    struct Options
    {
        /**
         * Source for the shader. If the source string is not empty, it will
         * used, otherwise the specified path will be used.
         */
        std::string             source;
        Path                    path;

        std::string             function;
        GPUShaderStage          stage;
    };

    using SourceList          = std::list<Path>;

public:
                                ShaderCompiler(Options inOptions);
                                ~ShaderCompiler() {}

public:
    /** Compile the shader. Use IsCompiled() to check the result. */
    void                        Compile();

    bool                        IsCompiled() const      { return !mCode.empty(); }

    /** After compilation, gets the generated SPIR-V code. */
    const GPUShaderCode&        GetCode() const         { Assert(IsCompiled()); return mCode; }
    GPUShaderCode               MoveCode() const        { Assert(IsCompiled()); return std::move(mCode); }

    /** After compilation, gets a list of source files referenced by the shader. */
    const SourceList&           GetSourceFiles() const  { return mSourceFiles; }

    static bool                 CompileFile(Path                 inPath,
                                            std::string          inFunction,
                                            const GPUShaderStage inStage,
                                            GPUShaderCode&       outCode);

    static bool                 CompileString(std::string          inSource,
                                              std::string          inFunction,
                                              const GPUShaderStage inStage,
                                              GPUShaderCode&       outCode);

private:
    bool                        LoadSource(const Path&  inPath,
                                           const Path&  inFrom,
                                           const size_t inLineIndex,
                                           const size_t inDepth,
                                           std::string& outSource);

    bool                        Preprocess(std::string& ioSource,
                                           const Path&  inPath,
                                           const size_t inDepth);

    bool                        GenerateSource(std::string& outSource);

private:
    Options                     mOptions;
    GPUShaderCode               mCode;
    SourceList                  mSourceFiles;

};
