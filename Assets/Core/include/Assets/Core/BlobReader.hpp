#pragma once

#include <Assets/Core/BlobView.hpp>

#include <filesystem>

namespace shuttle::assets::core
{
    template <typename T> std::optional<std::span<const T>> readTypedSection(
        const BlobView& blob,
        BlobSectionType type
    ) {
        const std::optional<BlobSection> section = blob.findSection(type);

        if (!section) return {};
        const std::span<const uint8_t> bytes = blob.bytes(*section);
        if (bytes.empty()) return {};
        if (bytes.size_bytes() % sizeof(T) != 0) return std::nullopt;

        return std::span<const T>(reinterpret_cast<const T*>(bytes.data()), bytes.size_bytes() / sizeof(T));
    }

    inline std::optional<std::span<const uint8_t>> readRawSection(
        const BlobView& blob,
        BlobSectionType type
    ) {
        const std::optional<BlobSection> section = blob.findSection(type);

        if (!section) return std::nullopt;

        return blob.bytes(*section);
    }

    class BlobReader
    {
    public:
        [[nodiscard]] static BlobView open(const std::filesystem::path& path);
    };
} // namespace shuttle::assets::core