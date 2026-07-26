#pragma once

#include "Importers/Assimp/AssimpSceneImporter.hpp"

#include "Builders/SceneBuilder.hpp"
#include "Builders/GeometryBuilder.hpp"
#include "Builders/MaterialBuilder.hpp"
#include "Builders/AnimationBuilder.hpp"
#include "Builders/SceneGraphBuilder.hpp"

#include "Texture/SceneTextureResolver.hpp"
#include "Texture/SceneTextureCompiler.hpp"

#include "Runtime/CompiledScene.hpp"
#include "Assets/SceneCompiler/SceneCompiler.hpp"

#include "Importers/Assimp/AssimpSceneImporter.hpp"
#include "Runtime/CompiledScene.hpp"
#include "Texture/SceneTextureResolver.hpp"
#include <filesystem>
#include <optional>

#include "Builders/GeometryBuilder.hpp"
#include "Texture/SceneTextureCompiler.hpp"

namespace shuttle::assets::scene_compiler
{
    struct SceneCompilerOptions
    {
        SceneTextureResolverOptions
            textureResolverOptions;

        SceneTextureCompilerOptions
            textureCompilerOptions;

        GeometryBuilderOptions
            geometryOptions;
    };

    class SceneCompiler
    {
    public:
        [[nodiscard]]
        static std::optional<CompiledScene> compile(
            const std::filesystem::path& scenePath,
            const SceneCompilerOptions& options = {});
    };
}
