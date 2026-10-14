//
// Created by Shagu on 04.08.2026.
//

#include "MainPass.hpp"

#include "Render.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine::render {

    void MainPassSettingSystem::updateSettings(MainPassSettings const &settings) const {
        std::memcpy(mainPassSettingsMappedPtr, &settings, sizeof(MainPassSettings));
    }

    vk::ResultValue<MainPassSettingSystem> MainPassSettingSystem::create(
        vk::Device device,
        vk::detail::DispatchLoaderDynamic const& dispatcher,
        resources::DeviceAllocator const& allocator
    ){
        auto [createMainPassSettingsBufferResult, mainPassSettingsBuffer] = allocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = sizeof(MainPassSettings),
                .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
                .sharingMode = vk::SharingMode::eExclusive
            },
            resources::MemoryUsage::eCpuToGpu,
            static_cast<resources::AllocationCreateFlagBits>(
                static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped) |
                static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite))
        );
        if (createMainPassSettingsBufferResult != vk::Result::eSuccess) {
            return {createMainPassSettingsBufferResult, {}};
        }

        vk::DeviceAddress const mainPassSettingsBufferAddress = device.getBufferAddress({.buffer = *mainPassSettingsBuffer}, dispatcher);
        void* const mainPassSettingsMappedPtr = allocator.getMappedPointer(*mainPassSettingsBuffer);

        if (mainPassSettingsMappedPtr == nullptr) {
            return {vk::Result::eErrorMemoryMapFailed, {}};
        }
        return {vk::Result::eSuccess, MainPassSettingSystem{
            std::move(mainPassSettingsBuffer),
            mainPassSettingsBufferAddress,
            mainPassSettingsMappedPtr
        }};
    }
}
