#pragma once

#include "IncludeVulkan.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Render.hpp"
#include "DescriptorHeapSet.hpp"

namespace shuttle::engine::render
{
    struct FallbackTextureIndices
    {
        uint32_t albedo{};
        uint32_t normal{};
        uint32_t orm{};
        uint32_t emission{};
    };

    struct FallbackTextureStorage
    {
        Texture albedoTexture;
        Texture normalTexture;
        Texture ormTexture;
        Texture emissionTexture;
    };

    struct FallbackTextures
    {
        FallbackTextureStorage storage;
        FallbackTextureIndices indices;
    };

    [[nodiscard]]
    vk::ResultValue<FallbackTextures> createFallbackTextures(
        vk::Device device,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator,
        DescriptorHeapSet& descriptorHeap);
}
