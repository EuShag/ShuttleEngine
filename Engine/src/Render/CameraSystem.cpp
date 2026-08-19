//
// Created by Shagu on 05.08.2026.
//

#include "CameraSystem.hpp"

namespace shuttle::engine::render {
    vk::ResultValue<CameraSystem> CameraSystem::create(vk::Device device, resources::DeviceAllocator const &allocator) {
        auto [createCameraDataBufferResult, cameraDataBuffer] = allocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = sizeof(core::CameraData),
                .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
                .sharingMode = vk::SharingMode::eExclusive
            },
            resources::MemoryUsage::eCpuToGpu,
            static_cast<resources::AllocationCreateFlagBits>(
                static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped) |
                static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite))
        );
        if (createCameraDataBufferResult != vk::Result::eSuccess) {
            return {createCameraDataBufferResult, {}};
        }

        void* mappedMemory = allocator.getMappedPointer(*cameraDataBuffer);
        if (mappedMemory == nullptr) {
            return {vk::Result::eErrorMemoryMapFailed, {}};
        }

        auto cameraDataDeviceAddress = device.getBufferAddress({.buffer = *cameraDataBuffer});

        return {vk::Result::eSuccess, CameraSystem{
                std::move(cameraDataBuffer),
                mappedMemory,
                cameraDataDeviceAddress
            }
        };
    }

    void CameraSystem::updateData(core::Camera const &camera) const {
        auto cameraData = camera.buildCameraData();
        std::memcpy(mappedMemory, &cameraData, sizeof(core::CameraData));
    }

}