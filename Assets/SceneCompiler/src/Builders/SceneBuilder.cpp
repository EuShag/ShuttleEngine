#include "SceneBuilder.hpp"

#include <algorithm>
#include <iostream>

#include "../../include/Assets/SceneCompiler/CompiledScene.hpp"

namespace shuttle::assets::scene_compiler
{
CompiledScene SceneBuilder::build(SceneTextureCompilerResult textures, MaterialBuildResult materials,
                                  GeometryBuildResult geometry,
                                  SceneGraphBuildResult sceneGraph, LightingBuildResult lighting)
{
    CompiledScene result{};

    //
    // textures
    //

    result.textures = std::move(textures.textures);

    //
    // materials
    //

    result.materials = std::move(materials.materials);

    //
    // geometry
    //

    result.positions = std::move(geometry.positions);

    result.attributes = std::move(geometry.attributes);

    result.skins = std::move(geometry.skins);

    result.indices = std::move(geometry.indices);

    result.meshes = std::move(geometry.meshes);

    //
    // scene graph
    //

    result.nodes = std::move(sceneGraph.nodes);

    result.levels = std::move(sceneGraph.levels);

    result.drawableObjects = std::move(sceneGraph.drawableObjects);

    result.transforms = std::move(sceneGraph.transforms);

    //
    // lighting
    //

    result.directionalLights = std::move(lighting.directionalLights);

    std::cout << "mesh count: " << result.meshes.size() << std::endl;

    return result;
}
} // namespace shuttle::assets::scene_compiler
