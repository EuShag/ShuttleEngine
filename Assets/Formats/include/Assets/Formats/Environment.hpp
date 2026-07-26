#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace shuttle::assets::formats::environment
{
    enum class EnvironmentFlags : uint32_t
    {
        None          = 0,
        VisibleSkybox = 1u << 0,
        UseTint       = 1u << 1,
        RotateSkybox  = 1u << 2
    };

    struct alignas(16) EnvironmentInfo
    {
        uint32_t nameHash{};

        int32_t skyboxTextureIndex{};
        int32_t irradianceTextureIndex{};
        int32_t prefilteredTextureIndex{};

        float intensity{};
        float skyboxIntensity{};
        float rotationYRadians{};

        uint32_t flags{};

        glm::vec4 tint{};
    };

    static_assert(sizeof(EnvironmentInfo) == 48);
}