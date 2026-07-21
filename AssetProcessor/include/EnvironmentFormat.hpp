//
// Created by Shagu on 21.07.2026.
//

#ifndef SHUTTLEENGINE_ENVIRONMENTBLOB_HPP
#define SHUTTLEENGINE_ENVIRONMENTBLOB_HPP
#include <glm/glm.hpp>
namespace shuttle_engine::format {


    enum class EnvironmentFlags : uint32_t
    {
        eNone = 0,
        eVisibleSkybox = 1 << 0,
        eUseTint = 1 << 1,
        eRotateSkybox = 1 << 2
    };


    struct alignas(16) EnvironmentBlobHeader {
        char magic[4] = {'E', 'N', 'V', 'B'};
        uint32_t version{};

        uint64_t environmentTableOffset{};
        uint32_t environmentCount{};
        uint32_t padding0{};

        uint64_t textureTableOffset{};
        uint32_t textureTableCount{};
        uint32_t padding1{};

        uint64_t bulkDataOffset{};
        uint64_t bulkDataSize{};
        uint64_t padding2{};
    };


    struct alignas(16) EnvironmentInfo
    {
        uint32_t nameHash{};               // 4

        int32_t skyboxTextureIdx{};        // 4
        int32_t irradianceTextureIdx{};    // 4
        int32_t prefilteredTextureIdx{};   // 4

        float intensity{};                 // 4
        float skyboxIntensity{};           // 4
        float rotationYRadians{};          // 4

        uint32_t flags{};                  // 4

        glm::vec4 tint{};                  // 16
    };
    static_assert(sizeof(EnvironmentBlobHeader) == 64);
    static_assert(sizeof(EnvironmentInfo) == 48);

}

#endif //SHUTTLEENGINE_ENVIRONMENTBLOB_HPP
