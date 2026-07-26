#pragma once

#include "../Intermediate/ImportedScene.hpp"

#include <Assets/TextureCompiler/CompiledTexture.hpp>
#include <Assets/TextureCompiler/TextureCompileOptions.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace shuttle::assets::scene_compiler
{
    struct SceneTextureCompilerOptions
    {
        bool compileTextures = true;

        bool generateOrmTextures = true;

        bool generateMips = true;

        bool flipY = false;

        bool roughnessIsGloss = false;
    };

    struct SceneTextureCompilerResult
    {
        bool success = true;

        std::string errorMessage;

        std::vector<texture_compiler::CompiledTexture> textures;

        std::vector<int32_t> importedToCompiledTexture;
    };

    class SceneTextureCompiler
    {
    public:
        [[nodiscard]]
        static SceneTextureCompilerResult compile(
            ImportedScene& scene,
            const SceneTextureCompilerOptions& options = {});
    };
}