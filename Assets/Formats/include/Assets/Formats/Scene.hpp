#pragma once

#include <Assets/Formats/Common.hpp>

namespace shuttle::assets::formats::scene
{
    struct alignas(16) NodeLevelRange
    {
        uint32_t startNodeIndex{};
        uint32_t nodeCount{};

        uint64_t reserved{};
    };

    struct alignas(16) SceneNode
    {
        glm::vec3 localTranslation{};
        uint32_t parentIndex{formats::InvalidIndexU32};

        glm::vec4 localRotationQuat{0.f, 0.f, 0.f, 1.f};

        glm::vec3 localScale{1.f};
        uint32_t flags{};

        uint32_t nodeNameHash{};

        uint32_t firstDrawableObject{};
        uint32_t drawableObjectCount{};

        uint64_t reserved{};
    };

    struct alignas(16) GpuDrawableObject
    {
        uint32_t sceneNodeIndex{};
        uint32_t meshIndex{};
        uint32_t materialIndex{};
        uint32_t flags{};
    };
}