#pragma once

#include "../Intermediate/ImportedScene.hpp"

#include <Assets/Formats/Common.hpp>
#include <Assets/Formats/Geometry.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace shuttle::assets::scene_compiler
{
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

struct GeometryBuildResult
{
    bool success = true;

    std::string errorMessage;

    std::vector<formats::PositionAttribute> positions;

    std::vector<formats::VertexAttribute> attributes;

    std::vector<formats::VertexSkin> skins;

    std::vector<uint32_t> indices;

    std::vector<formats::geometry::GpuMesh> meshes;

    std::vector<int32_t> importedToCompiledMesh;
};

class GeometryBuilder
{
  public:
    [[nodiscard]]
    static GeometryBuildResult build(const ImportedScene& scene, const GeometryBuilderOptions& options = {});
};
} // namespace shuttle::assets::scene_compiler