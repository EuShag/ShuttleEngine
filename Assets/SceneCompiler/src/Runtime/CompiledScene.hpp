#pragma once

#include <Assets/Formats/Animation.hpp>
#include <Assets/Formats/Geometry.hpp>
#include <Assets/Formats/Material.hpp>
#include <Assets/Formats/Scene.hpp>
#include <Assets/TextureCompiler/CompiledTexture.hpp>

namespace shuttle::assets::scene_compiler
{
    struct CompiledScene
    {
        //
        // textures
        //

        std::vector<
            texture_compiler::CompiledTexture>
            textures;

        //
        // materials
        //

        std::vector<
            formats::material::MaterialInfo>
            materials;

        //
        // geometry
        //

        std::vector<
            formats::PositionAttribute>
            positions;

        std::vector<
            formats::VertexAttribute>
            attributes;

        std::vector<
            formats::VertexSkin>
            skins;

        std::vector<uint32_t>
            indices;

        std::vector<
            formats::geometry::GpuMesh>
            meshes;

        //
        // scene graph
        //

        std::vector<
            formats::scene::SceneNode>
            nodes;

        std::vector<
            formats::scene::NodeLevelRange>
            levels;

        std::vector<
            formats::scene::GpuDrawableObject>
            drawableObjects;

        //
        // animation
        //

        std::vector<
            formats::animation::SkeletonData>
            skeletons;

        std::vector<
            formats::animation::BoneData>
            bones;

        std::vector<
            formats::animation::AnimationClip>
            clips;

        std::vector<
            formats::animation::BoneChannel>
            boneChannels;

        std::vector<
            formats::animation::MorphTarget>
            morphTargets;

        std::vector<
            formats::animation::MorphVertexDelta>
            morphVertexDeltas;

        std::vector<
            formats::animation::MorphChannel>
            morphChannels;

        std::vector<
            formats::animation::MaterialProperty>
            materialProperties;

        std::vector<
            formats::animation::MaterialChannel>
            materialChannels;

        std::vector<float>
            keyframeTimes;

        std::vector<
            formats::animation::AnimationKeyframeValue>
            keyframeValues;
    };
}
