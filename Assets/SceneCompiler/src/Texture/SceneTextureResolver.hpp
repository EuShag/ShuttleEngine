#pragma once

#include "../Intermediate/ImportedScene.hpp"

#include <filesystem>

namespace shuttle::assets::scene_compiler
{
struct SceneTextureResolverOptions
{
    bool scanSourceDirectory = true;

    bool resolveAlbedo = true;
    bool resolveNormal = true;
    bool resolveOrm = true;

    bool resolveOcclusion = true;
    bool resolveRoughness = true;
    bool resolveMetallic = true;

    bool resolveEmissiveFromCatalog = false;
};

class SceneTextureResolver
{
  public:
    static void resolve(ImportedScene& scene, const std::filesystem::path& sourceDirectory,
                        const SceneTextureResolverOptions& options = {});
};
} // namespace shuttle::assets::scene_compiler