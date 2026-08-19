#pragma once

#include "Assets/SceneCompiler/SceneCompiler.hpp"
#include "../Intermediate/ImportedScene.hpp"

#include <filesystem>

namespace shuttle::assets::scene_compiler
{

class SceneTextureResolver
{
  public:
    static void resolve(ImportedScene& scene, const std::filesystem::path& sourceDirectory,
                        const SceneTextureResolverOptions& options = {});
};
} // namespace shuttle::assets::scene_compiler