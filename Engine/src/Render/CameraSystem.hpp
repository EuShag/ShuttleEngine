//
// Created by Shagu on 05.08.2026.
//

#ifndef SHUTTLEENGINE_CAMERASYSTEM_HPP
#define SHUTTLEENGINE_CAMERASYSTEM_HPP

#include "IncludeVulkan.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Camera/Camera.hpp"

namespace shuttle::engine::render {
    class CameraSystem
    {
    public:

        static vk::ResultValue<CameraSystem> create(vk::Device device, resources::DeviceAllocator const& allocator);

        void updateData(core::Camera const& camera) const;

        [[nodiscard]] vk::DeviceAddress getCameraDataAddress() const {
            return cameraDataAddress;
        }

        CameraSystem(resources::UniqueAllocatedBuffer cameraDataBuffer, void* cameraDataMappedPtr, vk::DeviceAddress cameraDataAddress)
            : cameraDataBuffer(std::move(cameraDataBuffer)),
              mappedMemory(cameraDataMappedPtr),
              cameraDataAddress(cameraDataAddress) {}

        CameraSystem() = default;

    private:
        resources::UniqueAllocatedBuffer cameraDataBuffer{};
        void* mappedMemory{};
        vk::DeviceAddress cameraDataAddress{};
    };
}

#endif //SHUTTLEENGINE_CAMERASYSTEM_HPP
