//
// Created by Shagu on 08.04.2026.
//

#ifndef HELLOTRIANGLE_TEXTURE_HPP
#define HELLOTRIANGLE_TEXTURE_HPP
#include <vulkan/vulkan.h>

#include "ImageLoader/Image.hpp"

#endif //HELLOTRIANGLE_TEXTURE_HPP

namespace shuttle_engine {
    enum class TextureType {
        texture1D,
        texture2D,
        texture3D,
        textureCube
    };

    struct TextureProperties {
        vk::Format imageFormat;
    };

    class Texture {
    public:
        Texture (std::string const& filePath);

    private:
        vk::Image image;
        vk::ImageView view;
        vk::ImageLayout currentLayout;
        vk::ImageType imageType;
        vk::ImageViewType viewType;
    };

}
