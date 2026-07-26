#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace shuttle::assets::formats::texture
{
    enum TextureIndices : uint32_t
    {
        FallbackAlbedo   = 0,
        FallbackNormal   = 1,
        FallbackORM      = 2,
        FallbackEmissive = 3,

        FirstUserTexture = 4
    };

    enum class TextureSemantic : uint32_t
    {
        Albedo,
        Normal,
        ORM,
        Emissive,
        IBL,
        Unknown
    };

    enum class ImageType : uint32_t
    {
        Image1D,
        Image2D,
        Image3D
    };

    enum class ImageViewType : uint32_t
    {
        View1D,
        View2D,
        View3D,
        ViewCube,

        View1DArray,
        View2DArray,
        ViewCubeArray
    };

    struct alignas(16) TextureMetadata
    {
        uint64_t mipTableOffset{};

        uint32_t mipCount{};

        uint32_t width{};
        uint32_t height{};
        uint32_t depth{};

        uint32_t layerCount{};

        VkFormat format{VK_FORMAT_BC7_SRGB_BLOCK};

        ImageType imageType{
            ImageType::Image2D
        };

        ImageViewType imageViewType{
            ImageViewType::View2D
        };

        uint32_t reserved{};
    };

    struct alignas(16) TextureMipMetadata
    {
        uint64_t dataOffset{};
        uint64_t dataSize{};

        uint32_t width{};
        uint32_t height{};

        uint32_t reserved[2]{};
    };

    static_assert(sizeof(TextureMipMetadata) == 32);
}
