//
// Created by Shagu on 14.09.2026.
//

#ifndef SHUTTLEENGINE_ENVIRONMENTDATALOADER_HPP
#define SHUTTLEENGINE_ENVIRONMENTDATALOADER_HPP
#include <filesystem>
#include <span>
#include <vulkan/vulkan.hpp>

#include "assets/formats/environment.hpp"
#include "assets/formats/texture.hpp"

namespace shuttle::engine::render {
    struct LoadedEnvironmentData {
        std::span<assets::formats::environment::EnvironmentInfo const> environmentInfos;
        std::span<const assets::formats::texture::TextureMetadata> textureMetadatas;
        std::span<const assets::formats::texture::TextureMipMetadata> textureMipMetadatas;
        std::span<const uint8_t> textureBytes;
    };

    [[nodiscard]] vk::ResultValue<LoadedEnvironmentData> loadEnvironmentData(std::filesystem::path const& path);
}


#endif //SHUTTLEENGINE_ENVIRONMENTDATALOADER_HPP
