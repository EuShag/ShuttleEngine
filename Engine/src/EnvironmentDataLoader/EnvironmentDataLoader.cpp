//
// Created by Shagu on 14.09.2026.
//

#include "EnvironmentDataLoader.hpp"

#include "Assets/Core/BlobReader.hpp"

namespace shuttle::engine::render {

    vk::ResultValue<LoadedEnvironmentData> loadEnvironmentData(std::filesystem::path const &path) {
        auto blob = assets::core::BlobReader::open(path);

        LoadedEnvironmentData result{};

        auto environmentInfosOpt = readTypedSection<assets::formats::environment::EnvironmentInfo>(blob, assets::core::BlobSectionType::EnvironmentInfo);
        if (!environmentInfosOpt.has_value()) return { vk::Result::eErrorInitializationFailed, {} };
        result.environmentInfos = environmentInfosOpt.value();

        auto textureMetadatasOpt = readTypedSection<assets::formats::texture::TextureMetadata>(blob, assets::core::BlobSectionType::TextureMetadata);
        if (!textureMetadatasOpt.has_value()) return { vk::Result::eErrorInitializationFailed, {} };
        result.textureMetadatas = textureMetadatasOpt.value();

        auto textureMipMetadatasOpt = readTypedSection<assets::formats::texture::TextureMipMetadata>(blob, assets::core::BlobSectionType::TextureMipMetadata);
        if (!textureMipMetadatasOpt.has_value()) return { vk::Result::eErrorInitializationFailed, {} };
        result.textureMipMetadatas = textureMipMetadatasOpt.value();

        auto textureBytesOpt = readRawSection(blob, assets::core::BlobSectionType::TextureData);
        if (!textureBytesOpt.has_value()) return { vk::Result::eErrorInitializationFailed, {} };
        result.textureBytes = textureBytesOpt.value();

        return {vk::Result::eSuccess, result};
    }
}
