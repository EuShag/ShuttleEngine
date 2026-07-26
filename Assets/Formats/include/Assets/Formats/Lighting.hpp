#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace shuttle::assets::formats::lighting
{
    struct alignas(16) DirectionalLight
    {
        glm::vec4 directionAndIntensity{};
        glm::vec3 color{};
        uint32_t castShadows{};
    };

    static_assert(sizeof(DirectionalLight) == 32);

    struct alignas(16) PointLight
    {
        glm::vec4 positionAndRadius{};
        glm::vec3 color{};
        float intensity{};
    };

    static_assert(sizeof(PointLight) == 32);

    struct alignas(16) SpotLight
    {
        glm::vec4 positionAndRadius{};
        glm::vec4 directionAndIntensity{};

        glm::vec3 color{};
        float innerCutoffCos{};

        float outerCutoffCos{};
        uint32_t castShadows{};

        uint32_t reserved[2]{};
    };

    static_assert(sizeof(SpotLight) == 64);
}