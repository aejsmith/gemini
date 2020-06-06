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

#pragma once

#include "Core/Path.h"

#include "Render/ShaderManager.h"

/**
 * Class for compiling HLSL shaders to SPIR-V. Note that while we are currently
 * doing compilation entirely at runtime, it is intended in future that "baked"
 * game data would include only the pre-compiled target API shaders, and the
 * compiler would not be available in the final game/engine build.
 */
class ShaderCompiler
{
public:

    using SourceSet           = HashSet<Path>;

public:
                                ShaderCompiler(const ShaderKey&     key,
                                               const GPUShaderStage stage);
                                ~ShaderCompiler() {}

    /** Compile the shader. Use IsCompiled() to check the result. */
    void                        Compile();

    bool                        IsCompiled() const      { return !mCode.empty(); }

    /** After compilation, gets the generated SPIR-V code. */
    const GPUShaderCode&        GetCode() const         { Assert(IsCompiled()); return mCode; }
    GPUShaderCode               MoveCode() const        { Assert(IsCompiled()); return std::move(mCode); }

    /** After compilation, gets a list of source files referenced by the shader. */
    const SourceSet&            GetSourceFiles() const  { return mSourceFiles; }

private:
    bool                        LoadSource(const Path&  path,
                                           const Path&  from,
                                           const size_t lineIndex,
                                           const size_t depth,
                                           std::string& outSource);

    bool                        Preprocess(std::string& ioSource,
                                           const Path&  path,
                                           const size_t depth);

    bool                        GenerateSource();

private:
    const ShaderKey&            mKey;
    const GPUShaderStage        mStage;

    std::string                 mSource;
    GPUShaderCode               mCode;
    SourceSet                   mSourceFiles;

};
