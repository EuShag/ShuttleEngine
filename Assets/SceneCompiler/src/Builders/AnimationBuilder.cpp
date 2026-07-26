#include "AnimationBuilder.hpp"

#include <Assets/Formats/Common.hpp>

#include <algorithm>

namespace shuttle::assets::scene_compiler
{
    namespace
    {
        namespace animation_format =
            formats::animation;

        constexpr uint32_t InvalidU32 =
            formats::InvalidIndexU32;

        constexpr int32_t InvalidI32 =
            formats::InvalidIndexI32;

        uint32_t fnv1aHash(
            const std::string& text)
        {
            constexpr uint32_t basis =
                2166136261u;

            constexpr uint32_t prime =
                16777619u;

            uint32_t hash =
                basis;

            for (unsigned char c : text)
            {
                hash ^= c;
                hash *= prime;
            }

            return hash;
        }

        bool validNodeIndex(
            const ImportedScene& scene,
            uint32_t nodeIndex)
        {
            return nodeIndex != InvalidU32 &&
                nodeIndex < scene.nodes.size();
        }

        bool validMeshIndex(
            const ImportedScene& scene,
            int32_t meshIndex)
        {
            return meshIndex >= 0 &&
                meshIndex < static_cast<int32_t>(scene.meshes.size());
        }

        glm::quat safeNormalize(
            const glm::quat& value)
        {
            const float len =
                glm::length(value);

            if (len <= 0.000001f)
            {
                return glm::quat(
                    1.0f,
                    0.0f,
                    0.0f,
                    0.0f);
            }

            return glm::normalize(value);
        }

        void buildSkeletons(
            const ImportedScene& scene,
            AnimationBuildResult& result)
        {
            result.importedSkinToSkeleton.resize(
                scene.skins.size(),
                InvalidI32);

            result.skeletons.reserve(
                scene.skins.size());

            for (size_t skinIndex = 0;
                 skinIndex < scene.skins.size();
                 ++skinIndex)
            {
                const ImportedSkin& importedSkin =
                    scene.skins[skinIndex];

                if (importedSkin.bones.empty())
                {
                    continue;
                }

                animation_format::SkeletonData skeleton{};
                skeleton.boneOffset =
                    static_cast<uint32_t>(
                        result.bones.size());

                skeleton.boneCount =
                    static_cast<uint32_t>(
                        importedSkin.bones.size());

                skeleton.rootBoneIndex =
                    importedSkin.skeletonRoot != InvalidU32
                        ? static_cast<int32_t>(importedSkin.skeletonRoot)
                        : InvalidI32;

                for (const ImportedBone& importedBone :
                     importedSkin.bones)
                {
                    animation_format::BoneData bone{};

                    bone.invBindMatrix =
                        importedBone.inverseBindMatrix;

                    bone.parentBoneIndex =
                        importedBone.parentBone != InvalidU32
                            ? static_cast<int32_t>(importedBone.parentBone)
                            : InvalidI32;

                    bone.sceneNodeIndex =
                        importedBone.nodeIndex;

                    result.bones.push_back(
                        bone);
                }

                const int32_t skeletonIndex =
                    static_cast<int32_t>(
                        result.skeletons.size());

                result.skeletons.push_back(
                    skeleton);

                result.importedSkinToSkeleton[skinIndex] =
                    skeletonIndex;
            }
        }

        void buildMorphTargets(
            const ImportedScene& scene,
            AnimationBuildResult& result)
        {
            result.importedMeshMorphToRuntimeMorph.resize(
                scene.meshes.size());

            for (size_t meshIndex = 0;
                 meshIndex < scene.meshes.size();
                 ++meshIndex)
            {
                const ImportedMesh& mesh =
                    scene.meshes[meshIndex];

                result.importedMeshMorphToRuntimeMorph[meshIndex].resize(
                    mesh.morphTargets.size(),
                    InvalidU32);

                for (size_t morphIndex = 0;
                     morphIndex < mesh.morphTargets.size();
                     ++morphIndex)
                {
                    const ImportedMorphTarget& importedMorph =
                        mesh.morphTargets[morphIndex];

                    animation_format::MorphTarget morph{};

                    morph.firstDeltaIndex =
                        static_cast<uint32_t>(
                            result.morphVertexDeltas.size());

                    morph.deltaCount =
                        static_cast<uint32_t>(
                            importedMorph.deltas.size());

                    morph.targetNameHash =
                        fnv1aHash(
                            importedMorph.name);

                    morph.maxPositionDelta =
                        importedMorph.maxPositionDelta;

                    for (const ImportedMorphVertexDelta& importedDelta :
                         importedMorph.deltas)
                    {
                        animation_format::MorphVertexDelta delta{};

                        delta.positionDelta =
                            importedDelta.positionDelta;

                        delta.vertexIndex =
                            importedDelta.vertexIndex;

                        result.morphVertexDeltas.push_back(
                            delta);
                    }

                    const uint32_t runtimeMorphIndex =
                        static_cast<uint32_t>(
                            result.morphTargets.size());

                    result.morphTargets.push_back(
                        morph);

                    result.importedMeshMorphToRuntimeMorph[meshIndex][morphIndex] =
                        runtimeMorphIndex;
                }
            }
        }

        void appendBoneChannel(
            uint32_t nodeIndex,
            animation_format::AnimationPath path,
            uint32_t keyframeOffset,
            uint32_t keyframeCount,
            AnimationBuildResult& result)
        {
            animation_format::BoneChannel channel{};

            channel.boneIndex =
                nodeIndex;

            channel.path =
                path;

            channel.keyframeOffset =
                keyframeOffset;

            channel.keyframeCount =
                keyframeCount;

            result.boneChannels.push_back(
                channel);
        }

        void appendPositionKeys(
            uint32_t nodeIndex,
            const std::vector<PositionKey>& keys,
            AnimationBuildResult& result)
        {
            if (keys.empty())
            {
                return;
            }

            const uint32_t offset =
                static_cast<uint32_t>(
                    result.keyframeTimes.size());

            for (const PositionKey& key : keys)
            {
                result.keyframeTimes.push_back(
                    static_cast<float>(key.time));

                result.keyframeValues.push_back(
                    animation_format::AnimationKeyframeValue{
                        .value =
                            glm::vec4(
                                key.value,
                                1.0f)
                    });
            }

            appendBoneChannel(
                nodeIndex,
                animation_format::AnimationPath::Translation,
                offset,
                static_cast<uint32_t>(keys.size()),
                result);
        }

        void appendRotationKeys(
            uint32_t nodeIndex,
            const std::vector<RotationKey>& keys,
            AnimationBuildResult& result)
        {
            if (keys.empty())
            {
                return;
            }

            const uint32_t offset =
                static_cast<uint32_t>(
                    result.keyframeTimes.size());

            for (const RotationKey& key : keys)
            {
                const glm::quat rotation =
                    safeNormalize(
                        key.value);

                result.keyframeTimes.push_back(
                    static_cast<float>(key.time));

                result.keyframeValues.push_back(
                    animation_format::AnimationKeyframeValue{
                        .value =
                            glm::vec4(
                                rotation.x,
                                rotation.y,
                                rotation.z,
                                rotation.w)
                    });
            }

            appendBoneChannel(
                nodeIndex,
                animation_format::AnimationPath::Rotation,
                offset,
                static_cast<uint32_t>(keys.size()),
                result);
        }

        void appendScaleKeys(
            uint32_t nodeIndex,
            const std::vector<ScaleKey>& keys,
            AnimationBuildResult& result)
        {
            if (keys.empty())
            {
                return;
            }

            const uint32_t offset =
                static_cast<uint32_t>(
                    result.keyframeTimes.size());

            for (const ScaleKey& key : keys)
            {
                result.keyframeTimes.push_back(
                    static_cast<float>(key.time));

                result.keyframeValues.push_back(
                    animation_format::AnimationKeyframeValue{
                        .value =
                            glm::vec4(
                                key.value,
                                1.0f)
                    });
            }

            appendBoneChannel(
                nodeIndex,
                animation_format::AnimationPath::Scale,
                offset,
                static_cast<uint32_t>(keys.size()),
                result);
        }

        uint32_t findRuntimeMorphTargetForNodeWeight(
            const ImportedScene& scene,
            const AnimationBuildResult& result,
            uint32_t nodeIndex,
            size_t weightIndex)
        {
            if (!validNodeIndex(scene, nodeIndex))
            {
                return InvalidU32;
            }

            const ImportedNode& node =
                scene.nodes[nodeIndex];

            for (int32_t meshIndex : node.meshes)
            {
                if (!validMeshIndex(scene, meshIndex))
                {
                    continue;
                }

                const auto& morphMap =
                    result.importedMeshMorphToRuntimeMorph[
                        static_cast<size_t>(meshIndex)];

                if (weightIndex >= morphMap.size())
                {
                    continue;
                }

                const uint32_t runtimeMorphIndex =
                    morphMap[weightIndex];

                if (runtimeMorphIndex != InvalidU32)
                {
                    return runtimeMorphIndex;
                }
            }

            return InvalidU32;
        }

        size_t findMaxWeightCount(
            const std::vector<WeightKey>& keys)
        {
            size_t result = 0;

            for (const WeightKey& key : keys)
            {
                result =
                    std::max(
                        result,
                        key.value.size());
            }

            return result;
        }

        void appendMorphWeightChannels(
            const ImportedScene& scene,
            const ImportedAnimationChannel& importedChannel,
            AnimationBuildResult& result,
            uint32_t& activeMorphChannels)
        {
            if (importedChannel.weights.empty())
            {
                return;
            }

            const size_t weightCount =
                findMaxWeightCount(
                    importedChannel.weights);

            if (weightCount == 0)
            {
                return;
            }

            for (size_t weightIndex = 0;
                 weightIndex < weightCount;
                 ++weightIndex)
            {
                const uint32_t runtimeMorphIndex =
                    findRuntimeMorphTargetForNodeWeight(
                        scene,
                        result,
                        importedChannel.nodeIndex,
                        weightIndex);

                if (runtimeMorphIndex == InvalidU32)
                {
                    continue;
                }

                animation_format::MorphChannel morphChannel{};

                morphChannel.morphTargetIndex =
                    runtimeMorphIndex;

                morphChannel.keyframeOffset =
                    static_cast<uint32_t>(
                        result.keyframeTimes.size());

                morphChannel.keyframeCount =
                    static_cast<uint32_t>(
                        importedChannel.weights.size());

                for (const WeightKey& key :
                     importedChannel.weights)
                {
                    const float value =
                        weightIndex < key.value.size()
                            ? key.value[weightIndex]
                            : 0.0f;

                    result.keyframeTimes.push_back(
                        static_cast<float>(key.time));

                    result.keyframeValues.push_back(
                        animation_format::AnimationKeyframeValue{
                            .value =
                                glm::vec4(
                                    value,
                                    0.0f,
                                    0.0f,
                                    0.0f)
                        });
                }

                result.morphChannels.push_back(
                    morphChannel);

                ++activeMorphChannels;
            }
        }

        void buildAnimationClips(
            const ImportedScene& scene,
            AnimationBuildResult& result)
        {
            result.clips.reserve(
                scene.animations.size());

            for (const ImportedAnimation& importedAnimation :
                 scene.animations)
            {
                animation_format::AnimationClip clip{};

                clip.duration =
                    static_cast<float>(
                        importedAnimation.duration);

                clip.clipNameHash =
                    fnv1aHash(
                        importedAnimation.name);

                clip.firstBoneChannelIndex =
                    static_cast<uint32_t>(
                        result.boneChannels.size());

                clip.firstMorphChannelIndex =
                    static_cast<uint32_t>(
                        result.morphChannels.size());

                clip.firstMaterialChannelIndex =
                    static_cast<uint32_t>(
                        result.materialChannels.size());

                uint32_t activeBoneChannels = 0;
                uint32_t activeMorphChannels = 0;

                for (const ImportedAnimationChannel& channel :
                     importedAnimation.channels)
                {
                    if (!validNodeIndex(scene, channel.nodeIndex))
                    {
                        continue;
                    }

                    const uint32_t boneChannelsBefore =
                        static_cast<uint32_t>(
                            result.boneChannels.size());

                    appendPositionKeys(
                        channel.nodeIndex,
                        channel.positions,
                        result);

                    appendRotationKeys(
                        channel.nodeIndex,
                        channel.rotations,
                        result);

                    appendScaleKeys(
                        channel.nodeIndex,
                        channel.scales,
                        result);

                    activeBoneChannels +=
                        static_cast<uint32_t>(
                            result.boneChannels.size()) -
                        boneChannelsBefore;

                    appendMorphWeightChannels(
                        scene,
                        channel,
                        result,
                        activeMorphChannels);
                }

                clip.boneChannelCount =
                    activeBoneChannels;

                clip.morphChannelCount =
                    activeMorphChannels;

                clip.materialChannelCount =
                    0;

                result.clips.push_back(
                    clip);
            }
        }
    }

    AnimationBuildResult AnimationBuilder::build(
        const ImportedScene& scene)
    {
        AnimationBuildResult result{};

        buildSkeletons(
            scene,
            result);

        buildMorphTargets(
            scene,
            result);

        buildAnimationClips(
            scene,
            result);

        return result;
    }
}