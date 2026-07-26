#include "SceneBuilder.hpp"

#include <algorithm>

#include "Runtime/CompiledScene.hpp"

namespace shuttle::assets::scene_compiler
{
    CompiledScene SceneBuilder::build(
        SceneTextureCompilerResult textures,
        MaterialBuildResult materials,
        GeometryBuildResult geometry,
        AnimationBuildResult animation,
        SceneGraphBuildResult sceneGraph,
        LightingBuildResult lighting)
    {
        CompiledScene result{};

        //
        // textures
        //

        result.textures =
            std::move(
                textures.textures);

        //
        // materials
        //

        result.materials =
            std::move(
                materials.materials);

        //
        // geometry
        //

        result.positions =
            std::move(
                geometry.positions);

        result.attributes =
            std::move(
                geometry.attributes);

        result.skins =
            std::move(
                geometry.skins);

        result.indices =
            std::move(
                geometry.indices);

        result.meshes =
            std::move(
                geometry.meshes);

        //
        // scene graph
        //

        result.nodes =
            std::move(
                sceneGraph.nodes);

        result.levels =
            std::move(
                sceneGraph.levels);

        result.drawableObjects =
            std::move(
                sceneGraph.drawableObjects);

        //
        // skeletons
        //

        result.skeletons =
            std::move(
                animation.skeletons);

        result.bones =
            std::move(
                animation.bones);

        //
        // animation clips
        //

        result.clips =
            std::move(
                animation.clips);

        result.boneChannels =
            std::move(
                animation.boneChannels);

        //
        // morphing
        //

        result.morphTargets =
            std::move(
                animation.morphTargets);

        result.morphVertexDeltas =
            std::move(
                animation.morphVertexDeltas);

        result.morphChannels =
            std::move(
                animation.morphChannels);

        //
        // material animation
        //

        result.materialProperties =
            std::move(
                animation.materialProperties);

        result.materialChannels =
            std::move(
                animation.materialChannels);

        //
        // keyframes
        //

        result.keyframeTimes =
            std::move(
                animation.keyframeTimes);

        result.keyframeValues =
            std::move(
                animation.keyframeValues);

        //
        // lighting
        //

        result.directionalLights =
            std::move(
                lighting.directionalLights);

        return result;
    }
}
