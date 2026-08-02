#pragma once

#include "../Intermediate/ImportedScene.hpp"
#include "GeometryBuilder.hpp"
#include "MaterialBuilder.hpp"

#include "Assets/Formats/Scene.hpp"

namespace shuttle::assets::scene_compiler
{
struct SceneGraphBuildResult
{
    std::vector<formats::scene::SceneNode> nodes;

    std::vector<formats::scene::NodeLevelRange> levels;

    std::vector<formats::scene::Transform> transforms;

    std::vector<formats::scene::GpuDrawableObject> drawableObjects;

    std::vector<int32_t> importedNodeToRuntimeNode;
};

class SceneGraphBuilder
{
  public:
    [[nodiscard]]
    static SceneGraphBuildResult build(const ImportedScene& scene, const GeometryBuildResult& geometry,
                                       const MaterialBuildResult& materials);
};
} // namespace shuttle::assets::scene_compiler
