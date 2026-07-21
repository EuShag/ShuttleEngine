//
// Created by Shagu on 21.07.2026.
//

#include "EnvironmentBlobWriter.hpp"

#include <fstream>

#include "BlobLayout.hpp"

namespace shuttle_engine::format {
    struct TextureMetaData;

    TextureMetaData createTextureMetaData(
    const assets::EnvironmentTextureData& texture,
    uint64_t textureOffset)
    {
        TextureMetaData meta{};

        meta.textureOffset = textureOffset;
        meta.textureSize = texture.data.size();
        meta.format = texture.format;

        meta.width = texture.width;
        meta.height = texture.height;

        meta.mipCount = texture.mipCount;
        meta.numLayers = texture.faceCount;

        meta.isCubemap = texture.faceCount == 6;

        return meta;
    }
}



bool shuttle_engine::assets::EnvironmentBlobWriter::write(
    const EnvironmentBakeResult& bakeResult,
    const std::filesystem::path& outputPath)
{
    constexpr uint32_t kEnvironmentFormatRGBE = 1;

    format::EnvironmentBlobHeader header{};
    header.version = 1;

    std::vector<format::EnvironmentInfo> environments(1);

    auto& envInfo = environments.front();

    envInfo.nameHash = 0;

    envInfo.skyboxTextureIdx = 0;
    envInfo.irradianceTextureIdx = 1;
    envInfo.prefilteredTextureIdx = 2;

    envInfo.intensity = 1.0f;
    envInfo.skyboxIntensity = 1.0f;
    envInfo.rotationYRadians = 0.0f;

    envInfo.flags = static_cast<uint32_t>(
        format::EnvironmentFlags::eVisibleSkybox
    );

    std::vector<format::TextureMetaData> textures(3);

    const uint64_t envTableSize =
        sizeof(format::EnvironmentInfo);

    const uint64_t texTableSize =
        sizeof(format::TextureMetaData) * textures.size();

    header.environmentTableOffset =
        sizeof(format::EnvironmentBlobHeader);

    header.environmentCount = 1;

    header.textureTableOffset =
        header.environmentTableOffset +
        envTableSize;

    header.textureTableCount =
        static_cast<uint32_t>(textures.size());

    header.bulkDataOffset =
        header.textureTableOffset +
        texTableSize;

    uint64_t currentOffset = 0;


    textures[0] = format::createTextureMetaData(
        bakeResult.skybox,
        currentOffset
    );

    currentOffset += bakeResult.skybox.data.size();

    textures[1] = format::createTextureMetaData(
        bakeResult.irradiance,
        currentOffset
    );

    currentOffset += bakeResult.irradiance.data.size();

    textures[2] = format::createTextureMetaData(
        bakeResult.radiance,
        currentOffset
    );

    currentOffset += bakeResult.radiance.data.size();

    header.bulkDataSize = currentOffset;

    std::ofstream file(
        outputPath,
        std::ios::binary
    );

    if (!file)
    {
        return false;
    }

    file.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );

    file.write(
        reinterpret_cast<const char*>(environments.data()),
        sizeof(format::EnvironmentInfo) *
        environments.size()
    );

    file.write(
        reinterpret_cast<const char*>(textures.data()),
        sizeof(format::TextureMetaData) *
        textures.size()
    );


    file.write(
        reinterpret_cast<const char*>(bakeResult.skybox.data.data()),
        bakeResult.skybox.data.size()
    );

    file.write(
        reinterpret_cast<const char*>(bakeResult.irradiance.data.data()),
        bakeResult.irradiance.data.size()
    );

    file.write(
        reinterpret_cast<const char*>(bakeResult.radiance.data.data()),
        bakeResult.radiance.data.size()
    );


    return file.good();
}
