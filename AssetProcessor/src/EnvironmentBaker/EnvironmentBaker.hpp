#pragma once

#include <filesystem>
#include <vector>
#include <cmft/image.h>

namespace shuttle_engine::assets
{
    struct EnvironmentTextureData
    {
        uint32_t width{};
        uint32_t height{};
        uint32_t mipCount{};
        uint32_t faceCount{};

        uint32_t format{}; // VkFormat as uint32_t

        std::vector<std::byte> data;
    };

    struct EnvironmentBakeResult
    {
        EnvironmentTextureData skybox;
        EnvironmentTextureData irradiance;
        EnvironmentTextureData radiance;
    };

    class EnvironmentBaker
    {
    public:

        EnvironmentBakeResult bake(
            const std::filesystem::path& hdrFile);

    private:

        static cmft::CrtAllocator* allocator()
        {
            return &cmft::g_crtAllocator;
        }
    };
}