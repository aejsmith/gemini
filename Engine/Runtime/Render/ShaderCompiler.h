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
 * Class for compiling GLSL shaders to SPIR-V. All of our shaders are written
 * as GLSL using Vulkan semantics. Other target APIs have an additional
 * compilation step to go from SPIR-V to the correct target language.
 *
 * Note that while we are currently doing compilation entirely at runtime, it
 * is intended in future that "baked" game data would include only the pre-
 * compiled target API shaders, and the compiler would not be available in the
 * final game/engine build.
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

        GPUShaderStage          stage;
    };

    using SourceList          = std::list<Path>;

public:
                                ShaderCompiler(Options inOptions);
                                ~ShaderCompiler() {}

public:
    /** Compile the shader. Use IsCompiled() to check the result. */
    void                        Compile();

    bool                        IsCompiled() const      { return !mSPIRV.empty(); }

    /** After compilation, gets the generated SPIR-V code. */
    const SPIRVCode&            GetSPIRV() const        { Assert(IsCompiled()); return mSPIRV; }
    SPIRVCode                   MoveSPIRV() const       { Assert(IsCompiled()); return std::move(mSPIRV); }

    /** After compilation, gets a list of source files referenced by the shader. */
    const SourceList&           GetSourceFiles() const  { return mSourceFiles; }

    static bool                 CompileFile(Path                 inPath,
                                            const GPUShaderStage inStage,
                                            SPIRVCode&           outSPIRV);

    static bool                 CompileString(std::string          inSource,
                                              const GPUShaderStage inStage,
                                              SPIRVCode&           outSPIRV);

private:
    class SourceIncluder;
    friend class SourceIncluder;

private:
    std::string                 GenerateSource();

private:
    Options                     mOptions;
    SPIRVCode                   mSPIRV;
    SourceList                  mSourceFiles;

};
