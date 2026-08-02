#include <Assets/Core/BlobView.hpp>

#include <cstring>
#include <stdexcept>
#include <system_error>

namespace shuttle::assets::core
{
BlobView BlobView::open(const std::filesystem::path& path)
{
    BlobView view{};

    std::error_code error;

    view.m_mapping.map(path.string(), error);

    if (error) throw std::runtime_error("BlobView: failed to map file: " + error.message());
    if (view.m_mapping.empty()) throw std::runtime_error("BlobView: file is empty.");

    view.m_data = reinterpret_cast<const uint8_t*>(view.m_mapping.data());
    view.m_size = view.m_mapping.size();

    if (view.m_size < sizeof(BlobHeader))
    {
        throw std::runtime_error("BlobView: file is smaller than BlobHeader.");
    }

    view.m_header = reinterpret_cast<const BlobHeader*>(view.m_data);

    if (std::memcmp(view.m_header->magic, BlobMagic, 4) != 0) throw std::runtime_error("BlobView: invalid blob magic.");

    if (view.m_header->totalFileSize == 0 || view.m_header->totalFileSize > view.m_size)
    {
        throw std::runtime_error("BlobView: invalid total file size.");
    }

    view.m_containers =
        view.readSpan<ContainerReference>(view.m_header->containerTableOffset, view.m_header->containerCount);

    view.m_sections = view.readSpan<BlobSection>(view.m_header->sectionTableOffset, view.m_header->sectionCount);

    view.validateRange(view.m_header->stringTableOffset, view.m_header->stringTableSize);

    view.m_strings = StringTableView(view.m_data + view.m_header->stringTableOffset, view.m_header->stringTableSize);

    return view;
}

const BlobHeader& BlobView::header() const noexcept
{
    return *m_header;
}

std::span<const BlobSection> BlobView::sections() const noexcept
{
    return m_sections;
}

std::span<const ContainerReference> BlobView::containers() const noexcept
{
    return m_containers;
}

StringTableView BlobView::strings() const noexcept
{
    return m_strings;
}

std::optional<BlobSection> BlobView::findSection(BlobSectionType type, uint32_t occurrence) const
{
    uint32_t currentOccurrence = 0;

    for (const BlobSection& section : m_sections)
    {
        if (section.type != type) continue;
        if (currentOccurrence == occurrence) return section;
        ++currentOccurrence;
    }

    return std::nullopt;
}

std::span<const uint8_t> BlobView::bytes(const BlobSection& section) const
{
    if (section.containerIndex != 0) throw std::runtime_error("BlobView: section is stored in external container.");
    return bytesAt(section.offset, section.size);
}

std::span<const uint8_t> BlobView::bytesAt(uint64_t offset, uint64_t size) const
{
    if (size == 0) return {};
    validateRange(offset, size);
    return {m_data + offset, size};
}

const uint8_t* BlobView::data() const noexcept
{
    return m_data;
}

uint64_t BlobView::size() const noexcept
{
    return m_size;
}

void BlobView::validateRange(uint64_t offset, uint64_t size) const
{
    if (offset > m_size) throw std::runtime_error("BlobView: offset outside file.");
    if (size > m_size - offset) throw std::runtime_error("BlobView: range outside file.");
}
} // namespace shuttle::assets::core