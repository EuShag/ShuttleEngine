#pragma once

#include "include_vulkan.hpp"
#include "device_allocator/device_allocator.hpp"
#include "render.hpp"
#include "descriptor_heap_set.hpp"

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
}
