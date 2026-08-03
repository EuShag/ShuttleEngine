#pragma once

#include "../Intermediate/ImportedScene.hpp"
#include "../Texture/SceneTextureCompiler.hpp"

#include <Assets/Formats/Material.hpp>

namespace shuttle::assets::scene_compiler
{
struct MaterialBuildResult
{
    std::vector<formats::material::MaterialInfo> materials;

    std::vector<int32_t> importedToCompiledMaterial;
};

class MaterialBuilder
{
  public:
    [[nodiscard]]
    static MaterialBuildResult build(const ImportedScene& scene, const SceneTextureCompilerResult& textures);
};
} // namespace shuttle::assets::scene_compiler