#pragma once

#include <Assets/Core/BlobFormat.hpp>
#include <Assets/Core/ContainerTable.hpp>
#include <Assets/Core/StringTable.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace shuttle::assets::core {
    class BlobWriter {
    public:
        BlobWriter();

        [[nodiscard]] StringTableBuilder& strings() noexcept;
        [[nodiscard]] ContainerTableBuilder& containers() noexcept;

        void addSection(BlobSectionType type, std::span<const uint8_t> bytes, uint32_t flags = 0);

        template <typename T> void addTypedSection(BlobSectionType type, std::span<const T> values, uint32_t flags = 0) {
            const auto* raw = reinterpret_cast<const uint8_t*>(values.data());
            addSection(type, std::span<const uint8_t>(raw, values.size_bytes()), flags);
        }

        void addExternalSection(
            BlobSectionType type,
            uint32_t containerIndex,
            uint64_t offset,
            uint64_t size,
            uint32_t flags = 0);

        [[nodiscard]] bool write(const std::filesystem::path& path) const;

    private:
        struct PendingSection {
            BlobSectionType type{BlobSectionType::Unknown};
            uint32_t flags{};
            uint32_t containerIndex{};
            uint64_t externalOffset{};
            uint64_t externalSize{};
            std::vector<uint8_t> data;
        };

        StringTableBuilder m_strings;
        ContainerTableBuilder m_containers;
        std::vector<PendingSection> m_sections;
    };
}