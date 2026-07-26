#pragma once

#include "AnimationBuilder.hpp"
#include "GeometryBuilder.hpp"
#include "MaterialBuilder.hpp"
#include "SceneGraphBuilder.hpp"

#include "../Texture/SceneTextureCompiler.hpp"

#include "Runtime/CompiledScene.hpp"

namespace shuttle::assets::scene_compiler
{
    class SceneBuilder
    {
    public:
        [[nodiscard]]
        static CompiledScene build(
            SceneTextureCompilerResult textures,
            MaterialBuildResult materials,
            GeometryBuildResult geometry,
            AnimationBuildResult animation,
            SceneGraphBuildResult sceneGraph);
    };
}
