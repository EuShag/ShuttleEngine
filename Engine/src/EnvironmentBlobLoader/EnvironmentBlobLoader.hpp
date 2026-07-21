#pragma once

#include <filesystem>
#include <vector>
#include <span>
#include <cstddef>
#include <cstdint>

#include "BlobLayout.hpp"
#include "EnvironmentFormat.hpp"

namespace shuttle_engine::assets
{

    struct BlobEnvironmentTexture
    {
        const format::TextureMetaData* meta = nullptr;
        std::span<const std::byte> data{};

        [[nodiscard]] bool valid() const
        {
            return meta != nullptr && !data.empty();
        }

        [[nodiscard]] uint32_t width() const
        {
            return meta->width;
        }

        [[nodiscard]] uint32_t height() const
        {
            return meta->height;
        }

        [[nodiscard]] uint32_t mipCount() const
        {
            return meta->mipCount;
        }

        [[nodiscard]] uint32_t layerCount() const
        {
            return meta->numLayers;
        }

        [[nodiscard]] uint32_t format() const
        {
            return meta->format;
        }

        [[nodiscard]] bool isCubemap() const
        {
            return meta->isCubemap != 0;
        }
    };

    class BlobEnvironmentData
    {
    public:
        static BlobEnvironmentData loadFromFile(
            const std::filesystem::path& path
        );

        [[nodiscard]] bool valid() const;

        [[nodiscard]] const format::EnvironmentBlobHeader& header() const;
        [[nodiscard]] const format::EnvironmentInfo& environment() const;

        [[nodiscard]] BlobEnvironmentTexture skybox() const;
        [[nodiscard]] BlobEnvironmentTexture irradiance() const;
        [[nodiscard]] BlobEnvironmentTexture radiance() const;

        [[nodiscard]] const std::vector<format::TextureMetaData>& textures() const
        {
            return m_textures;
        }

    private:
        format::EnvironmentBlobHeader m_header{};
        format::EnvironmentInfo m_environment{};

        std::vector<format::TextureMetaData> m_textures;
        std::vector<std::byte> m_bulkData;

    private:
        [[nodiscard]] BlobEnvironmentTexture makeTexture(
            int32_t textureIndex
        ) const;

        [[nodiscard]] std::span<const std::byte> getTextureData(
            uint32_t textureIndex
        ) const;
    };
}
