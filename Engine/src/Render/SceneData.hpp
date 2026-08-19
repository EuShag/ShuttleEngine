    //
    // Created by Shagu on 07.08.2026.
    //

    #ifndef SHUTTLEENGINE_SCENEDATA_HPP
    #define SHUTTLEENGINE_SCENEDATA_HPP

    #include <IncludeVulkan.hpp>
    #include <glm/vec4.hpp>

    #include "Assets/Formats/Common.hpp"
    #include "Assets/Formats/Material.hpp"

    namespace shuttle::engine::render {

        struct alignas(16) SceneGpuInfo {
            vk::DeviceAddress sceneNodesBufferDeviceAddress;
            vk::DeviceAddress materialDatasBufferAddress;
            vk::DeviceAddress lightDatasBufferAddress;
            vk::DeviceAddress meshDatasBufferAddress;
            vk::DeviceAddress drawablesBufferAddress;
            vk::DeviceAddress localTransformsBufferAddress;

            uint32_t materialCount;
            uint32_t lightCount;
            uint32_t meshCount;
            uint32_t drawableCount;
            uint32_t nodeCount;
            uint32_t padding0;
        };

        struct alignas(16) EnvironmentGpuInfo
        {
            uint32_t skyboxTexture = UINT32_MAX;
            uint32_t irradianceTexture = UINT32_MAX;
            uint32_t radianceTexture = UINT32_MAX;
            uint32_t radianceMipLevels = 0;
        };

        static_assert(sizeof(EnvironmentGpuInfo) == 16);

        struct alignas(16) SceneNode
        {
            uint32_t parentIndex{assets::formats::InvalidIndexU32};
            uint32_t transformIndex{};
            uint32_t animationBindingIndex{assets::formats::InvalidIndexU32};
            uint32_t nodeNameHash{};
        };

        struct alignas(16) MeshLodGpuInfo
        {
            uint32_t firstIndex{};
            uint32_t indexCount{};

            float geometricError{};

            float screenThreshold{};
        };

        struct alignas(16) MeshGpuInfo  {
            vk::DeviceAddress positionAttributeBufferAddress;
            vk::DeviceAddress normalUvTangentAttributeBufferAddress;
            glm::vec4 boundingSphere{};
            glm::vec4 minAABB;
            glm::vec4 maxAABB;
            uint32_t meshFlags{};
            uint32_t lodCount{};
            uint64_t reserved{};
            MeshLodGpuInfo lods[4];
        };

        struct alignas(16) MaterialGpuInfo
        {
            glm::vec4 baseColorFactor{1.0f};
            glm::vec4 emissiveFactor{0.0f};

            float metallicFactor = 0.0f;
            float roughnessFactor = 1.0f;
            float alphaCutoff = 0.5f;
            float occlusionStrength = 1.0f;

            float emissiveStrength = 1.0f;

            uint32_t albedoTexture = UINT32_MAX;
            uint32_t normalTexture = UINT32_MAX;
            uint32_t ormTexture = UINT32_MAX;
            uint32_t emissiveTexture = UINT32_MAX;

            uint32_t flags = 0;
            uint32_t pipelineFlags = 0;
            assets::formats::material::AlphaMode alphaMode = assets::formats::material::AlphaMode::Opaque;
        };

        struct alignas(16) LocalTransformGpuInfo {
            glm::vec4 translation{0.0f};
            glm::vec4 rotation{0.0f};
            glm::vec4 scale{0.0f};
        };

        struct alignas(16) DrawableGpuInfo {
            uint32_t meshIndex{};
            uint32_t materialIndex{};
            uint32_t transformIndex{};
            uint32_t reserved{};
        };

        struct alignas(16) DirectionalLightGpuInfo {
            glm::vec4 lightDirection{0.0f};
            glm::vec4 lightColorAndIntensity{0.0f};
        };
    }

    #endif //SHUTTLEENGINE_SCENEDATA_HPP
