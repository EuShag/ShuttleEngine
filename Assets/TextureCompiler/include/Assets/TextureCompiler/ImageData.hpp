#pragma once

#include <cstddef>
#include <cstdint>

namespace shuttle::assets::texture_compiler
{
    struct ImageData
    {
        const uint8_t* pixels = nullptr;
        size_t size = 0;

        [[nodiscard]]
        bool valid() const noexcept
        {
            return pixels != nullptr && size > 0;
        }
    };

    struct ImageSizedData
    {
        const uint8_t* pixels = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;

        [[nodiscard]]
        bool valid() const noexcept
        {
            return pixels != nullptr && width > 0 && height > 0;
        }
    };

    enum class CompressionType : uint32_t
    {
        BC5,
        BC6H,
        BC7
    };
}