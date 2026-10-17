#pragma once

#include "include_vulkan.hpp"
#include "device_allocator/device_allocator.hpp"
#include "descriptor_heap_set.hpp"

#include <filesystem>

#include "render.hpp"

namespace shuttle::engine::render
{
    struct CommonResourceIndices
    {
        uint32_t brdfLutTexture{};

        uint32_t materialSampler{};
        uint32_t shadowSampler{};
        uint32_t nearestSampler{};
    };

    struct alignas(16) CommonResourcesInfo
    {
        uint32_t brdfLutTexture{};

        uint32_t materialSampler{};
        uint32_t shadowSampler{};
        uint32_t nearestSampler{};
    };

    static_assert(sizeof(CommonResourcesInfo) == 16);

    struct CommonResourceStorage
    {
        Texture brdfLutTexture;

        vk::UniqueSampler materialSampler;
        UniqueDescriptorSlot materialSamplerSlot;
        vk::UniqueSampler shadowSampler;
        UniqueDescriptorSlot shadowSamplerSlot;
        vk::UniqueSampler nearestSampler;
        UniqueDescriptorSlot nearestSamplerSlot;
    };

    struct CommonResources
    {
        CommonResourceStorage storage;
        CommonResourcesInfo info;
        resources::UniqueAllocatedBuffer infoBuffer;
        vk::DeviceAddress infoAddress{};
    };

    [[nodiscard]] vk::ResultValue<CommonResources> createCommonResources(
        vk::Device device,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator,
        DescriptorHeapSet& descriptorHeap,
        std::filesystem::path const& brdfLutPath =
            "../resources/engine/brdf_lut.dds");
}
