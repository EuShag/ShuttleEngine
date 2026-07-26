#include "Assets/SceneCompiler/SceneCompiler.hpp"

#include "Importers/Assimp/AssimpSceneImporter.hpp"
#include "Runtime/CompiledScene.hpp"
#include "Texture/SceneTextureResolver.hpp"
#include "Builders/LightingBuilder.hpp"

namespace shuttle::assets::scene_compiler
{
    std::optional<CompiledScene> SceneCompiler::compile(
        const std::filesystem::path& scenePath,
        const SceneCompilerOptions& options)
    {
        //
        // import
        //

        auto importedScene =
            AssimpSceneImporter::import(
                scenePath);

        if (!importedScene)
        {
            return std::nullopt;
        }

        //
        // texture resolving
        //

        SceneTextureResolver::resolve(
            *importedScene,
            scenePath.parent_path(),
            options.textureResolverOptions);

        //
        // textures
        //
        // textures
        //

        SceneTextureCompilerResult textures =
            SceneTextureCompiler::compile(
                *importedScene,
                options.textureCompilerOptions);

        if (!textures.success)
        {
            return std::nullopt;
        }

        //
        // geometry
        //

        GeometryBuildResult geometry =
            GeometryBuilder::build(
                *importedScene,
                options.geometryOptions);

        if (!geometry.success)
        {
            return std::nullopt;
        }

        //
        // ma*erials
        //

        MaterialBuildResult materials =
            MaterialBuilder::build(
                *importedScene,
                textures);

        //
        // animation
        //

        AnimationBuildResult animation =
            AnimationBuilder::build(
                *importedScene);

        if (!animation.success)
        {
            return std::nullopt;
        }

        //
        // scene graph
        //

        SceneGraphBuildResult graph =
            SceneGraphBuilder::build(
                *importedScene,
                geometry,
                materials);

        //
        // lighting
        //

        LightingBuildResult lighting =
            LightingBuilder::build(
                *importedScene);

        //
        // final scene
        //

        return SceneBuilder::build(
            std::move(textures),
            std::move(materials),
            std::move(geometry),
            std::move(animation),
            std::move(graph),
            std::move(lighting));
    }
}
