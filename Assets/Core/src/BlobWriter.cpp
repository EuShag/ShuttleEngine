#include <Assets/Core/BlobWriter.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace shuttle::assets::core
{
    namespace
    {
        constexpr uint64_t WriteChunkSize =
            64ull * 1024ull * 1024ull;

        [[nodiscard]]
        bool writeBytesAt(
            std::ofstream& stream,
            uint64_t offset,
            std::span<const uint8_t> bytes)
        {
            if (bytes.empty())
            {
                return true;
            }

            stream.seekp(
                static_cast<std::streamoff>(offset),
                std::ios::beg);

            if (!stream.good())
            {
                return false;
            }

            uint64_t written = 0;

            while (written < bytes.size())
            {
                const uint64_t remaining =
                    static_cast<uint64_t>(bytes.size()) -
                    written;

                const uint64_t toWrite =
                    std::min(
                        remaining,
                        WriteChunkSize);

                stream.write(
                    reinterpret_cast<const char*>(
                        bytes.data() + written),
                    static_cast<std::streamsize>(
                        toWrite));

                if (!stream.good())
                {
                    return false;
                }

                written += toWrite;
            }

            return true;
        }

        template<typename T>
        [[nodiscard]]
        bool writeObjectAt(
            std::ofstream& stream,
            uint64_t offset,
            const T& value)
        {
            const auto* raw =
                reinterpret_cast<const uint8_t*>(
                    &value);

            return writeBytesAt(
                stream,
                offset,
                std::span<const uint8_t>(
                    raw,
                    sizeof(T)));
        }

        template<typename T>
        [[nodiscard]]
        bool writeSpanAt(
            std::ofstream& stream,
            uint64_t offset,
            std::span<const T> values)
        {
            if (values.empty())
            {
                return true;
            }

            const auto* raw =
                reinterpret_cast<const uint8_t*>(
                    values.data());

            return writeBytesAt(
                stream,
                offset,
                std::span<const uint8_t>(
                    raw,
                    values.size_bytes()));
        }

        [[nodiscard]]
        bool validExternalContainerIndex(
            uint32_t index,
            uint32_t containerCount)
        {
            return index < containerCount;
        }
    }

    BlobWriter::BlobWriter()
    {
        m_containers.addSelf();
    }

    StringTableBuilder& BlobWriter::strings() noexcept
    {
        return m_strings;
    }

    ContainerTableBuilder& BlobWriter::containers() noexcept
    {
        return m_containers;
    }

    void BlobWriter::addSection(
        BlobSectionType type,
        std::span<const uint8_t> bytes,
        uint32_t flags)
    {
        PendingSection section{};
        section.type =
            type;

        section.flags =
            flags;

        section.containerIndex =
            0;

        section.externalOffset =
            0;

        section.externalSize =
            0;

        section.data.assign(
            bytes.begin(),
            bytes.end());

        m_sections.push_back(
            std::move(section));
    }

    void BlobWriter::addExternalSection(
        BlobSectionType type,
        uint32_t containerIndex,
        uint64_t offset,
        uint64_t size,
        uint32_t flags)
    {
        PendingSection section{};
        section.type =
            type;

        section.flags =
            flags |
            toRaw(BlobSectionFlags::External);

        section.containerIndex =
            containerIndex;

        section.externalOffset =
            offset;

        section.externalSize =
            size;

        m_sections.push_back(
            std::move(section));
    }

    bool BlobWriter::write(
        const std::filesystem::path& path) const
    {
        BlobHeader header{};

        std::memcpy(
            header.magic,
            BlobMagic,
            sizeof(header.magic));

        header.version =
            BlobFormatVersion;

        const auto& containerEntries =
            m_containers.entries();

        const std::vector<uint8_t>& stringBytes =
            m_strings.bytes();

        std::vector<BlobSection> finalSections;
        finalSections.resize(
            m_sections.size());

        uint64_t cursor =
            align16(sizeof(BlobHeader));

        // ---------------------------------------------------------------------
        // Container table
        // ---------------------------------------------------------------------

        if (!containerEntries.empty())
        {
            header.containerTableOffset =
                cursor;

            header.containerCount =
                static_cast<uint32_t>(
                    containerEntries.size());

            cursor +=
                static_cast<uint64_t>(
                    containerEntries.size()) *
                sizeof(ContainerReference);

            cursor =
                align16(cursor);
        }
        else
        {
            header.containerTableOffset =
                0;

            header.containerCount =
                0;
        }

        // ---------------------------------------------------------------------
        // Section table
        // ---------------------------------------------------------------------

        if (!finalSections.empty())
        {
            header.sectionTableOffset =
                cursor;

            header.sectionCount =
                static_cast<uint32_t>(
                    finalSections.size());

            cursor +=
                static_cast<uint64_t>(
                    finalSections.size()) *
                sizeof(BlobSection);

            cursor =
                align16(cursor);
        }
        else
        {
            header.sectionTableOffset =
                0;

            header.sectionCount =
                0;
        }

        // ---------------------------------------------------------------------
        // String table
        // ---------------------------------------------------------------------

        if (!stringBytes.empty())
        {
            header.stringTableOffset =
                cursor;

            header.stringTableSize =
                static_cast<uint64_t>(
                    stringBytes.size());

            cursor +=
                static_cast<uint64_t>(
                    stringBytes.size());

            cursor =
                align16(cursor);
        }
        else
        {
            header.stringTableOffset =
                0;

            header.stringTableSize =
                0;
        }

        // ---------------------------------------------------------------------
        // Section payloads
        // ---------------------------------------------------------------------

        for (size_t i = 0;
             i < m_sections.size();
             ++i)
        {
            const PendingSection& pending =
                m_sections[i];

            BlobSection section{};
            section.type =
                pending.type;

            section.flags =
                pending.flags;

            section.containerIndex =
                pending.containerIndex;

            if (hasFlag(pending.flags, BlobSectionFlags::External))
            {
                if (!validExternalContainerIndex(
                        pending.containerIndex,
                        header.containerCount))
                {
                    return false;
                }

                section.offset =
                    pending.externalOffset;

                section.size =
                    pending.externalSize;
            }
            else
            {
                section.containerIndex =
                    0;

                if (!pending.data.empty())
                {
                    cursor =
                        align16(cursor);

                    section.offset =
                        cursor;

                    section.size =
                        static_cast<uint64_t>(
                            pending.data.size());

                    cursor +=
                        static_cast<uint64_t>(
                            pending.data.size());

                    cursor =
                        align16(cursor);
                }
                else
                {
                    section.offset =
                        0;

                    section.size =
                        0;
                }
            }

            finalSections[i] =
                section;
        }

        header.totalFileSize =
            align16(cursor);

        // ---------------------------------------------------------------------
        // Write file
        // ---------------------------------------------------------------------

        std::ofstream out(
            path,
            std::ios::binary |
            std::ios::trunc);

        if (!out.is_open())
        {
            return false;
        }

        // Header
        if (!writeObjectAt(
                out,
                0,
                header))
        {
            return false;
        }

        // Containers
        if (!containerEntries.empty())
        {
            if (!writeSpanAt(
                    out,
                    header.containerTableOffset,
                    std::span<const ContainerReference>(
                        containerEntries.data(),
                        containerEntries.size())))
            {
                return false;
            }
        }

        // Sections
        if (!finalSections.empty())
        {
            if (!writeSpanAt(
                    out,
                    header.sectionTableOffset,
                    std::span<const BlobSection>(
                        finalSections.data(),
                        finalSections.size())))
            {
                return false;
            }
        }

        // Strings
        if (!stringBytes.empty())
        {
            if (!writeBytesAt(
                    out,
                    header.stringTableOffset,
                    std::span<const uint8_t>(
                        stringBytes.data(),
                        stringBytes.size())))
            {
                return false;
            }
        }

        // Section payloads
        for (size_t i = 0;
             i < m_sections.size();
             ++i)
        {
            const PendingSection& pending =
                m_sections[i];

            const BlobSection& section =
                finalSections[i];

            if (hasFlag(section.flags, BlobSectionFlags::External))
            {
                continue;
            }

            if (pending.data.empty())
            {
                continue;
            }

            if (!writeBytesAt(
                    out,
                    section.offset,
                    std::span<const uint8_t>(
                        pending.data.data(),
                        pending.data.size())))
            {
                return false;
            }
        }

        // Force final file size.
        if (header.totalFileSize > 0)
        {
            out.seekp(
                static_cast<std::streamoff>(
                    header.totalFileSize - 1),
                std::ios::beg);

            const char zero = 0;
            out.write(
                &zero,
                1);
        }

        out.flush();

        return out.good();
    }
}