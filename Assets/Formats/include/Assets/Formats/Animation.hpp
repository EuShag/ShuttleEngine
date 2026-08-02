#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include <Assets/Formats/Common.hpp>

namespace shuttle::assets::formats::animation
{
enum class AnimationPath : uint32_t
{
    Translation = 0,
    Rotation = 1,
    Scale = 2
};

struct alignas(16) SkeletonData
{
    uint32_t boneOffset{};
    uint32_t boneCount{};

    int32_t rootBoneIndex{-1};

    uint32_t reserved{};
};

static_assert(sizeof(SkeletonData) == 16);

struct alignas(16) BoneData
{
    glm::mat4 invBindMatrix{1.0f};

    int32_t parentBoneIndex{-1};

    uint32_t sceneNodeIndex{InvalidIndexU32};

    uint32_t reserved[2]{};
};

static_assert(sizeof(BoneData) == 80);

struct alignas(16) AnimationClip
{
    float duration{};

    uint32_t firstTransformChannelIndex{};
    uint32_t transformChannelCount{};

    uint32_t firstMorphChannelIndex{};
    uint32_t morphChannelCount{};

    uint32_t firstMaterialChannelIndex{};
    uint32_t materialChannelCount{};

    uint32_t clipNameHash{};
};

static_assert(sizeof(AnimationClip) == 32);

struct alignas(16) TransformChannel
{
    uint32_t transformBindingIndex{};

    AnimationPath path{AnimationPath::Translation};

    uint32_t keyframeOffset{};
    uint32_t keyframeCount{};
};

static_assert(sizeof(TransformChannel) == 16);

struct alignas(16) MorphChannel
{
    uint32_t morphTargetIndex{};

    uint32_t keyframeOffset{};
    uint32_t keyframeCount{};

    uint32_t flags{};
};

static_assert(sizeof(MorphChannel) == 16);

struct alignas(16) MaterialChannel
{
    uint32_t materialIndex{};

    uint32_t propertyHash{};

    uint32_t keyframeOffset{};
    uint32_t keyframeCount{};
};

static_assert(sizeof(MaterialChannel) == 16);

struct alignas(16) AnimationKeyframeValue
{
    glm::vec4 value{};
};

static_assert(sizeof(AnimationKeyframeValue) == 16);

struct alignas(16) MorphTarget
{
    uint32_t firstDeltaIndex{};
    uint32_t deltaCount{};

    uint32_t targetNameHash{};
    uint32_t reserved{};

    glm::vec3 maxPositionDelta{};
    uint32_t reserved2{};
};

static_assert(sizeof(MorphTarget) == 32);

struct alignas(16) MorphVertexDelta
{
    glm::vec3 positionDelta{};

    uint32_t vertexIndex{};
};

static_assert(sizeof(MorphVertexDelta) == 16);

struct alignas(16) MaterialProperty
{
    uint32_t propertyHash{};

    uint32_t elementCount{};

    uint32_t offsetInMaterial{};

    uint32_t reserved{};
};

static_assert(sizeof(MaterialProperty) == 16);
} // namespace shuttle::assets::formats::animation