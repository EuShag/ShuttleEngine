#pragma once

#include "CompiledScene.hpp"
#include "Assets/SceneCompiler/SceneCompiler.hpp"

#include <filesystem>
#include <optional>

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

    struct GeometryBuilderOptions
    {
        bool generateLods = true;

        bool optimizeVertexCache = true;
        bool optimizeVertexFetch = true;
        bool optimizeOverdraw = false;

        uint32_t maxLodCount = formats::geometry::MaxMeshLods;

        float lod1Ratio = 0.60f;
        float lod2Ratio = 0.30f;
        float lod3Ratio = 0.10f;

        float simplifyTargetError = 1e-2f;

        float lod0ScreenThreshold = 0.50f;
        float lod1ScreenThreshold = 0.20f;
        float lod2ScreenThreshold = 0.08f;
        float lod3ScreenThreshold = 0.01f;
    };

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
