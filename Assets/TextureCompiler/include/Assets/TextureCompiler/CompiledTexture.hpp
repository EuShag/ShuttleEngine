#pragma once

#include <Assets/Formats/Texture.hpp>

#include <cstdint>
#include <vector>

namespace shuttle::assets::texture_compiler
{
    struct CompiledTexture
    {
        std::vector<uint8_t> data;

        formats::texture::TextureMetadata metadata{};

        std::vector<formats::texture::TextureMipMetadata> mipMetadata;
    };
}