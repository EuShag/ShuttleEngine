#pragma once

#include <Assets/Formats/Texture.hpp>

namespace shuttle::assets::texture_compiler
{
struct TextureCompileOptions
{
    formats::texture::TextureSemantic semantic{formats::texture::TextureSemantic::Unknown};

    VkFormat format{VK_FORMAT_BC7_SRGB_BLOCK};

    bool generateMips = true;
    bool flipY = false;
    bool roughnessIsGloss = false;
};
} // namespace shuttle::assets::texture_compiler
