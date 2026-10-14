//
// Created by Shagu on 04.08.2026.
//

#ifndef SHUTTLEENGINE_MAINPASS_HPP
#define SHUTTLEENGINE_MAINPASS_HPP

#include "IncludeVulkan.hpp"
#include "Common.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include <glm/glm.hpp>

namespace shuttle::engine::render {

    enum class DebugMode : uint32_t {
        Final = 0,

        Albedo,
        Normal,
        Tangent,
        Bitangent,

        Metallic,
        Roughness,
        AmbientOcclusion,

        Emissive,

        UV,

        MeshId,
        MaterialId,
        InstanceId,

        ViewDepth,
        LinearDepth,

        WorldPosition,
        WorldNormal
    };

    struct alignas(16) MainPassSettings {
        // Resolution
        glm::vec2 renderResolution;
        glm::vec2 invRenderResolution;

        // Tonemapping
        float exposure = 0.03f;
        float gamma = 2.2f;

        // Image Based Lighting
        float diffuseIblStrength = 1.0f;
        float specularIblStrength = 1.0f;

        // Environment
        float skyboxIntensity = 1.0f;

        // Materials
        float emissiveIntensity = 1.0f;

        // Debug
        DebugMode debugModeOutput1{DebugMode::Final};
        DebugMode debugModeOutput2{DebugMode::Albedo};
        DebugMode debugModeOutput3{DebugMode::Normal};
        DebugMode debugModeOutput4{DebugMode::WorldPosition};

        // Reserved
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;
    };

    class MainPassSettingSystem {
    public:
        [[nodiscard]] static vk::ResultValue<MainPassSettingSystem> create(
            vk::Device device,
            vk::detail::DispatchLoaderDynamic const &dispatcher,
            resources::DeviceAllocator const& allocator);

        void updateSettings(MainPassSettings const& settings) const;

        [[nodiscard]] vk::DeviceAddress getMainPassSettingsBufferAddress() const {
            return mainPassSettingsBufferAddress;
        }

        MainPassSettingSystem() = default;
        MainPassSettingSystem(
            resources::UniqueAllocatedBuffer mainPassSettingsBuffer,
            vk::DeviceAddress mainPassSettingsBufferAddress,
            void* mainPassSettingsMappedPtr
        ) : mainPassSettingsBuffer(std::move(mainPassSettingsBuffer)),
            mainPassSettingsBufferAddress(mainPassSettingsBufferAddress),
            mainPassSettingsMappedPtr(mainPassSettingsMappedPtr) {}

        MainPassSettingSystem(const MainPassSettingSystem&) = delete;
        MainPassSettingSystem& operator=(const MainPassSettingSystem&) = delete;

        MainPassSettingSystem(MainPassSettingSystem&& other) noexcept :
            mainPassSettingsBuffer(std::move(other.mainPassSettingsBuffer)),
            mainPassSettingsBufferAddress(other.mainPassSettingsBufferAddress),
            mainPassSettingsMappedPtr(other.mainPassSettingsMappedPtr) {
                other.mainPassSettingsBufferAddress = 0;
                other.mainPassSettingsMappedPtr = nullptr;
        }

        MainPassSettingSystem& operator=(MainPassSettingSystem&& other) noexcept {
            if (this != &other) {
                mainPassSettingsBuffer = std::move(other.mainPassSettingsBuffer);
                mainPassSettingsBufferAddress = other.mainPassSettingsBufferAddress;
                mainPassSettingsMappedPtr = other.mainPassSettingsMappedPtr;

                other.mainPassSettingsBufferAddress = 0;
                other.mainPassSettingsMappedPtr = nullptr;
            }
            return *this;
        }

    private:

        resources::UniqueAllocatedBuffer mainPassSettingsBuffer;
        vk::DeviceAddress mainPassSettingsBufferAddress{};
        void* mainPassSettingsMappedPtr{};
    };
}
#endif //SHUTTLEENGINE_MAINPASS_HPP
