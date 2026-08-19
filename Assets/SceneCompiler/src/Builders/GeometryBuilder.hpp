#pragma once

#include "../Intermediate/ImportedScene.hpp"

#include <Assets/Formats/Common.hpp>
#include <Assets/Formats/Geometry.hpp>
#include <Assets/SceneCompiler/SceneCompiler.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace shuttle::assets::scene_compiler
{

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