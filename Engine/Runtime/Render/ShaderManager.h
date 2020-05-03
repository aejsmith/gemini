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

#include "Core/HashTable.h"
#include "Core/Path.h"
#include "Core/Singleton.h"

#include "GPU/GPUShader.h"

#include "Render/RenderDefs.h"

class ShaderTechnique;

/**
 * Class managing shaders. All shaders are loaded through this class.
 *
 * It provides a virtual filesystem for referring to shaders by a path string:
 * strings starting with "Engine/" map into the engine shader source directory,
 * while ones starting "Game/" map to the current game's shader source
 * directory.
 *
 * Development builds of the game compile shaders from source as needed. It is
 * intended in future that final game builds would pre-compile all needed
 * shaders from source to SPIR-V. Only the compiled binaries would be included
 * in the game data, not the source, and the shader compiler wouldn't even need
 * to be included in the game binary. The same shader paths and lookup
 * mechanism would retrieve pre-compiled binaries as opposed to compiling from
 * source. Shaders would be looked up based on an identifier derived from all
 * of the arguments to GetShader().
 */
class ShaderManager : public Singleton<ShaderManager>
{
public:
    using SearchPathMap   = HashMap<std::string, std::string>;

public:
                            ShaderManager();
                            ~ShaderManager();

    const SearchPathMap&    GetSearchPaths() const { return mSearchPaths; }

    /**
     * Get the specified shader from its virtual path and a function name
     * within that shader. When a technique is specified, the shader will be
     * compiled with parameter definitions derived from the technique's
     * parameters.
     */
    GPUShaderPtr            GetShader(const Path&                  path,
                                      const std::string&           function,
                                      const GPUShaderStage         stage,
                                      const ShaderTechnique* const technique = nullptr);

private:
    SearchPathMap           mSearchPaths;

};
