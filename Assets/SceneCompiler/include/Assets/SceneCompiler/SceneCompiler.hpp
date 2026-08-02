#pragma once

#include "Builders/GeometryBuilder.hpp"

#include "Texture/SceneTextureResolver.hpp"
#include "Texture/SceneTextureCompiler.hpp"

#include "Runtime/CompiledScene.hpp"
#include "Assets/SceneCompiler/SceneCompiler.hpp"

#include <filesystem>
#include <optional>

namespace shuttle::assets::scene_compiler
{
struct SceneCompilerOptions
{
    SceneTextureResolverOptions textureResolverOptions;

    SceneTextureCompilerOptions textureCompilerOptions;

    GeometryBuilderOptions geometryOptions;
};

class SceneCompiler
{
  public:
    [[nodiscard]]
    static std::optional<CompiledScene> compile(const std::filesystem::path& scenePath,
                                                const SceneCompilerOptions& options = {});
};
} // namespace shuttle::assets::scene_compiler
