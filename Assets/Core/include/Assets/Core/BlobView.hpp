#pragma once

#include <Assets/Core/BlobFormat.hpp>
#include <Assets/Core/StringTable.hpp>

#include <cstdint>
#include <filesystem>
#include <mio/mio.hpp>
#include <optional>
#include <span>

namespace shuttle::assets::core
{
class BlobView
{
  public:
    BlobView() = default;

    BlobView(const BlobView&) = delete;
    BlobView& operator=(const BlobView&) = delete;

    BlobView(BlobView&&) noexcept = default;
    BlobView& operator=(BlobView&&) noexcept = default;

    [[nodiscard]] static BlobView open(const std::filesystem::path& path);
    [[nodiscard]] const BlobHeader& header() const noexcept;
    [[nodiscard]] std::span<const BlobSection> sections() const noexcept;
    [[nodiscard]] std::span<const ContainerReference> containers() const noexcept;
    [[nodiscard]] StringTableView strings() const noexcept;
    [[nodiscard]] std::optional<BlobSection> findSection(BlobSectionType type, uint32_t occurrence = 0) const;
    [[nodiscard]] std::span<const uint8_t> bytes(const BlobSection& section) const;
    [[nodiscard]] std::span<const uint8_t> bytesAt(uint64_t offset, uint64_t size) const;
    [[nodiscard]] const uint8_t* data() const noexcept;
    [[nodiscard]] uint64_t size() const noexcept;

  private:
    template <typename T> [[nodiscard]] std::span<const T> readSpan(uint64_t offset, uint32_t count) const
    {
        if (count == 0) return {};

        const uint64_t sizeBytes = static_cast<uint64_t>(count) * sizeof(T);

        validateRange(offset, sizeBytes);

        return std::span<const T>(reinterpret_cast<const T*>(m_data + offset), count);
    }

    void validateRange(uint64_t offset, uint64_t size) const;

    mio::mmap_source m_mapping;

    const uint8_t* m_data = nullptr;
    uint64_t m_size = 0;

    const BlobHeader* m_header = nullptr;

    std::span<const BlobSection> m_sections;
    std::span<const ContainerReference> m_containers;

    StringTableView m_strings;
};
} // namespace shuttle::assets::core