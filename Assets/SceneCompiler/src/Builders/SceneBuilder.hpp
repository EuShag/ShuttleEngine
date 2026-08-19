#pragma once

#include "GeometryBuilder.hpp"
#include "MaterialBuilder.hpp"
#include "SceneGraphBuilder.hpp"
#include "LightingBuilder.hpp"

#include "../Texture/SceneTextureCompiler.hpp"

#include "../../include/Assets/SceneCompiler/CompiledScene.hpp"

namespace shuttle::assets::scene_compiler
{
class SceneBuilder
{
  public:
    [[nodiscard]]
    static CompiledScene build(SceneTextureCompilerResult textures, MaterialBuildResult materials,
                               GeometryBuildResult geometry,
                               SceneGraphBuildResult sceneGraph, LightingBuildResult lighting);
};
} // namespace shuttle::assets::scene_compiler
