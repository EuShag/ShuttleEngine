#include "EnvironmentBlobLoader.hpp"

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

#include "EnvironmentFormat.hpp"

namespace shuttle_engine::assets
{

    static std::vector<std::byte> readWholeFile(
        const std::filesystem::path& path
    )
    {
        std::ifstream file(
            path,
            std::ios::binary | std::ios::ate
        );

        if (!file)
        {
            throw std::runtime_error(
                "Failed to open environment blob: " + path.string()
            );
        }

        const std::streamsize fileSize = file.tellg();

        if (fileSize <= 0)
        {
            throw std::runtime_error(
                "Environment blob is empty: " + path.string()
            );
        }

        std::vector<std::byte> data(
            static_cast<size_t>(fileSize)
        );

        file.seekg(0);
        file.read(
            reinterpret_cast<char*>(data.data()),
            fileSize
        );

        if (!file)
        {
            throw std::runtime_error(
                "Failed to read environment blob: " + path.string()
            );
        }

        return data;
    }

    static void validateRange(
        uint64_t fileSize,
        uint64_t offset,
        uint64_t size,
        const char* name
    )
    {
        if (offset > fileSize)
        {
            throw std::runtime_error(
                std::string("Invalid offset for ") + name
            );
        }

        if (size > fileSize - offset)
        {
            throw std::runtime_error(
                std::string("Invalid range for ") + name
            );
        }
    }

}


namespace shuttle_engine::assets
{

    static bool hasEnvbMagic(
        const format::EnvironmentBlobHeader& header
    )
    {
        return
            header.magic[0] == 'E' &&
            header.magic[1] == 'N' &&
            header.magic[2] == 'V' &&
            header.magic[3] == 'B';
    }

    BlobEnvironmentData BlobEnvironmentData::loadFromFile(
        const std::filesystem::path& path
    )
    {
        BlobEnvironmentData result{};

        std::vector<std::byte> fileData =
            readWholeFile(path);

        const auto fileSize =
            static_cast<uint64_t>(fileData.size());

        validateRange(
            fileSize,
            0,
            sizeof(format::EnvironmentBlobHeader),
            "EnvironmentBlobHeader"
        );

        const auto* header =
            reinterpret_cast<const format::EnvironmentBlobHeader*>(
                fileData.data()
            );

        if (!hasEnvbMagic(*header))
        {
            throw std::runtime_error(
                "Invalid ENVB magic: " + path.string()
            );
        }

        if (header->version != 1)
        {
            throw std::runtime_error(
                "Unsupported ENVB version: " +
                std::to_string(header->version)
            );
        }

        if (header->environmentCount == 0)
        {
            throw std::runtime_error(
                "ENVB contains no environments."
            );
        }

        if (header->textureTableCount == 0)
        {
            throw std::runtime_error(
                "ENVB contains no textures."
            );
        }

        const uint64_t environmentTableSize =
            uint64_t(header->environmentCount) *
            sizeof(format::EnvironmentInfo);

        const uint64_t textureTableSize =
            uint64_t(header->textureTableCount) *
            sizeof(format::TextureMetaData);

        validateRange(
            fileSize,
            header->environmentTableOffset,
            environmentTableSize,
            "EnvironmentInfo table"
        );

        validateRange(
            fileSize,
            header->textureTableOffset,
            textureTableSize,
            "TextureMetaData table"
        );

        validateRange(
            fileSize,
            header->bulkDataOffset,
            header->bulkDataSize,
            "BulkData"
        );

        result.m_header = *header;

        const auto* environments =
            reinterpret_cast<const format::EnvironmentInfo*>(
                fileData.data() + header->environmentTableOffset
            );

        // Пока поддерживаем первое окружение.
        result.m_environment = environments[0];

        const auto* textures =
            reinterpret_cast<const format::TextureMetaData*>(
                fileData.data() + header->textureTableOffset
            );

        result.m_textures.assign(
            textures,
            textures + header->textureTableCount
        );

        result.m_bulkData.resize(
            static_cast<size_t>(header->bulkDataSize)
        );

        std::memcpy(
            result.m_bulkData.data(),
            fileData.data() + header->bulkDataOffset,
            static_cast<size_t>(header->bulkDataSize)
        );

        // Минимальная проверка индексов.
        auto checkTextureIndex = [&](int32_t index, const char* name)
        {
            if (index < 0)
            {
                throw std::runtime_error(
                    std::string("Missing environment texture: ") + name
                );
            }

            if (static_cast<uint32_t>(index) >= result.m_textures.size())
            {
                throw std::runtime_error(
                    std::string("Environment texture index out of range: ") + name
                );
            }
        };

        checkTextureIndex(
            result.m_environment.skyboxTextureIdx,
            "skybox"
        );

        checkTextureIndex(
            result.m_environment.irradianceTextureIdx,
            "irradiance"
        );

        checkTextureIndex(
            result.m_environment.prefilteredTextureIdx,
            "radiance"
        );

        // Проверяем, что данные реально доступны.
        if (!result.skybox().valid())
        {
            throw std::runtime_error("Invalid skybox texture in ENVB.");
        }

        if (!result.irradiance().valid())
        {
            throw std::runtime_error("Invalid irradiance texture in ENVB.");
        }

        if (!result.radiance().valid())
        {
            throw std::runtime_error("Invalid radiance texture in ENVB.");
        }

        return result;
    }

    bool BlobEnvironmentData::valid() const
    {
        return
            hasEnvbMagic(m_header) &&
            !m_textures.empty() &&
            !m_bulkData.empty();
    }

    const format::EnvironmentBlobHeader& BlobEnvironmentData::header() const
    {
        return m_header;
    }

    const format::EnvironmentInfo& BlobEnvironmentData::environment() const
    {
        return m_environment;
    }

    BlobEnvironmentTexture BlobEnvironmentData::skybox() const
    {
        return makeTexture(
            m_environment.skyboxTextureIdx
        );
    }

    BlobEnvironmentTexture BlobEnvironmentData::irradiance() const
    {
        return makeTexture(
            m_environment.irradianceTextureIdx
        );
    }

    BlobEnvironmentTexture BlobEnvironmentData::radiance() const
    {
        return makeTexture(
            m_environment.prefilteredTextureIdx
        );
    }

    BlobEnvironmentTexture BlobEnvironmentData::makeTexture(
        int32_t textureIndex
    ) const
    {
        if (textureIndex < 0)
        {
            return {};
        }

        const uint32_t index =
            static_cast<uint32_t>(textureIndex);

        if (index >= m_textures.size())
        {
            return {};
        }

        BlobEnvironmentTexture texture{};
        texture.meta = &m_textures[index];
        texture.data = getTextureData(index);

        return texture;
    }

    std::span<const std::byte> BlobEnvironmentData::getTextureData(
        uint32_t textureIndex
    ) const
    {
        if (textureIndex >= m_textures.size())
        {
            return {};
        }

        const auto& meta =
            m_textures[textureIndex];

        const uint64_t currentOffset =
            meta.textureOffset;

        if (currentOffset >= m_bulkData.size())
        {
            return {};
        }

        uint64_t nextOffset =
            static_cast<uint64_t>(m_bulkData.size());

        for (uint32_t i = 0; i < m_textures.size(); ++i)
        {
            if (i == textureIndex)
            {
                continue;
            }

            const uint64_t candidate =
                m_textures[i].textureOffset;

            if (candidate > currentOffset)
            {
                nextOffset = std::min(
                    nextOffset,
                    candidate
                );
            }
        }

        const uint64_t dataSize =
            nextOffset - currentOffset;

        if (dataSize == 0)
        {
            return {};
        }

        return std::span<const std::byte>(
            m_bulkData.data() + currentOffset,
            static_cast<size_t>(dataSize)
        );
    }
}

