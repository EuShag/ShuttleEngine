#pragma once

#include "../Intermediate/ImportedScene.hpp"

#include <Assets/Formats/Animation.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace shuttle::assets::scene_compiler
{
struct AnimationBuildResult
{
    bool success = true;

    std::string errorMessage;

    std::vector<formats::animation::SkeletonData> skeletons;

    std::vector<formats::animation::BoneData> bones;

    std::vector<formats::animation::AnimationClip> clips;

    std::vector<formats::animation::TransformChannel> transformChannels;

    std::vector<formats::animation::MorphTarget> morphTargets;

    std::vector<formats::animation::MorphVertexDelta> morphVertexDeltas;

    std::vector<formats::animation::MorphChannel> morphChannels;

    std::vector<formats::animation::MaterialProperty> materialProperties;

    std::vector<formats::animation::MaterialChannel> materialChannels;

    std::vector<float> keyframeTimes;

    std::vector<formats::animation::AnimationKeyframeValue> keyframeValues;

    std::vector<int32_t> importedSkinToSkeleton;

    std::vector<std::vector<uint32_t>> importedMeshMorphToRuntimeMorph;
};

class AnimationBuilder
{
  public:
    [[nodiscard]]
    static AnimationBuildResult build(const ImportedScene& scene);
};
} // namespace shuttle::assets::scene_compiler