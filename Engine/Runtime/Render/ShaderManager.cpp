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

#include "Render/ShaderManager.h"

#include "Core/Filesystem.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "GPU/GPUDevice.h"

#include "Render/ShaderCompiler.h"

SINGLETON_IMPL(ShaderManager);

ShaderManager::ShaderManager()
{
    mSearchPaths.emplace("Engine", "Engine/Shaders");

    const std::string gamePath = StringUtils::Format("Games/%s/Shaders", Platform::GetProgramName().c_str());
    mSearchPaths.emplace("Game", gamePath);

    LogDebug("Shader search paths:");
    for (const auto& it : mSearchPaths)
    {
        LogDebug("  %-6s = %s", it.first.c_str(), it.second.c_str());
    }
}

ShaderManager::~ShaderManager()
{
}

GPUShaderPtr ShaderManager::GetShader(const Path&                  path,
                                      const std::string&           function,
                                      const GPUShaderStage         stage,
                                      const ShaderTechnique* const technique)
{
    /*
     * TODO: Cache of GPUShaders. Would need a weak pointer stored in the cache
     * so that we can free shaders and pipelines when shaders are no longer
     * needed.
     */

    /* Turn the virtual path into a filesystem path. */
    auto searchPath = mSearchPaths.find(path.Subset(0, 1).GetString());
    if (searchPath == mSearchPaths.end())
    {
        LogError("Could not find shader '%s' (unknown search path)", path.GetCString());
        DebugBreak();
        return nullptr;
    }

    ShaderCompiler::Options options;
    options.function  = function;
    options.stage     = stage;
    options.technique = technique;

    options.path = Path(searchPath->second) / path.Subset(1);

    if (!Filesystem::Exists(options.path))
    {
        LogError("Could not find shader '%s' ('%s' does not exist)",
                 path.GetCString(),
                 options.path.GetCString());

        DebugBreak();
        return nullptr;
    }

    ShaderCompiler compiler(options);
    compiler.Compile();

    if (!compiler.IsCompiled())
    {
        LogError("Compilation of shader '%s' failed", path.GetCString());
        DebugBreak();
        return nullptr;
    }

    GPUShaderPtr shader = GPUDevice::Get().CreateShader(stage,
                                                        compiler.MoveCode(),
                                                        function);

    shader->SetName(path.GetString());

    return shader;
}
