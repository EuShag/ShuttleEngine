#pragma once

#include <Assets/Formats/Common.hpp>

namespace shuttle::assets::formats::scene
{
struct NodeLevelRange
{
    uint32_t startIndex{};
    uint32_t count{};
};

struct alignas(16) SceneNode
{
    uint32_t parentIndex{InvalidIndexU32};

    uint32_t transformIndex{};

    uint32_t animationBindingIndex{InvalidIndexU32};

    uint32_t nodeNameHash{};
};

static_assert(sizeof(SceneNode) == 16);

struct alignas(16) Transform
{
    glm::vec4 translation{};

    glm::vec4 rotationQuat{0.0f, 0.0f, 0.0f, 1.0f};

    glm::vec4 scale{1.0f};
};

static_assert(sizeof(Transform) == 48);

struct alignas(16) GpuDrawableObject
{
    uint32_t meshIndex{};

    uint32_t materialIndex{};

    uint32_t transformIndex{};

    uint32_t flags{};
};

static_assert(sizeof(GpuDrawableObject) == 16);
} // namespace shuttle::assets::formats::scene