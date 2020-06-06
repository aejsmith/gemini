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

size_t HashValue(const ShaderKey& key)
{
    size_t hash = HashValue(key.path);
    hash = HashCombine(hash, key.function);
    hash = HashCombine(hash, key.technique);
    hash = HashCombine(hash, key.features);

    for (const std::string& define : key.defines)
    {
        hash = HashCombine(hash, define);
    }

    return hash;
}

bool operator==(const ShaderKey& a, const ShaderKey& b)
{
    return a.path      == b.path &&
           a.function  == b.function &&
           a.defines   == b.defines &&
           a.technique == b.technique &&
           a.features  == b.features;
}

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
                                      const GPUShaderStage         stage)
{
    return GetShader(path, function, stage, {}, nullptr, 0);
}

GPUShaderPtr ShaderManager::GetShader(const Path&                  path,
                                      const std::string&           function,
                                      const GPUShaderStage         stage,
                                      const ShaderDefineArray&     defines,
                                      const ShaderTechnique* const technique,
                                      const uint32_t               features)
{
    ShaderKey key;
    key.path      = path.GetString();
    key.function  = function;
    key.defines   = defines;
    key.technique = technique;
    key.features  = features;

    /* Sort defines to make sure keys with the same defines match. */
    std::sort(key.defines.begin(), key.defines.end());

    GPUShaderPtr shader;

    /* Look up in the cache. */
    {
        std::shared_lock lock(mLock);

        auto it = mShaders.find(key);
        if (it != mShaders.end())
        {
            shader = it->second;
        }
    }

    if (!shader)
    {
        /* Compile shaders outside the lock to allow parallel shader compilation. */
        ShaderCompiler compiler(key, stage);
        compiler.Compile();

        if (compiler.IsCompiled())
        {
            shader = GPUDevice::Get().CreateShader(stage,
                                                   compiler.MoveCode(),
                                                   function);

            std::unique_lock lock(mLock);

            auto ret = mShaders.emplace(std::move(key), shader.Get());

            if (ret.second)
            {
                shader->SetName(path.GetString());

                shader->SetDestroyCallback(
                    [this, shaderPtr = shader.Get(), mapKey = &ret.first->first] ()
                    {
                        return RemoveShader(shaderPtr, *mapKey);
                    });
            }
            else
            {
                /* Another thread created the same shader and beat us to adding
                 * it to the cache. Use that one instead. */
                shader = ret.first->second;
            }
        }
        else
        {
            LogError("Compilation of shader '%s' failed", path.GetCString());
            DebugBreak();
        }
    }

    return shader;
}

bool ShaderManager::RemoveShader(GPUShader* const shader,
                                 const ShaderKey& key)
{
    std::unique_lock lock(mLock);

    /* Avoid a race where a shader's reference count reaches 0 on one thread
     * while another is getting it out of the cache. Now that we're holding the
     * lock, check the reference count again and if it's back above 0, don't
     * destroy the shader. */
    if (shader->GetRefCount() == 0)
    {
        const size_t count = mShaders.erase(key);
        Assert(count == 1);
        Unused(count);

        return true;
    }
    else
    {
        return false;
    }
}
