#include <Assets/Core/ContainerTable.hpp>

#include <stdexcept>

namespace shuttle::assets::core
{
    uint32_t ContainerTableBuilder::addSelf()
    {
        if (!m_entries.empty())
        {
            return 0;
        }

        ContainerReference self{};
        self.pathStringOffset = InvalidStringOffset;
        self.flags = 0;
        self.guidLow = 0;
        self.guidHigh = 0;
        self.reserved0 = 0;

        m_entries.push_back(self);

        return 0;
    }

    uint32_t ContainerTableBuilder::addExternalContainer(
        StringTableBuilder& strings,
        std::string_view path,
        uint64_t guidLow,
        uint64_t guidHigh,
        uint32_t flags)
    {
        ContainerReference entry{};
        entry.pathStringOffset = strings.addString(path);
        entry.flags = flags;
        entry.guidLow = guidLow;
        entry.guidHigh = guidHigh;
        entry.reserved0 = 0;

        const uint32_t index =
            static_cast<uint32_t>(m_entries.size());

        m_entries.push_back(entry);

        return index;
    }

    const std::vector<ContainerReference>&
    ContainerTableBuilder::entries() const noexcept
    {
        return m_entries;
    }

    bool ContainerTableBuilder::empty() const noexcept
    {
        return m_entries.empty();
    }

    void ContainerTableBuilder::clear()
    {
        m_entries.clear();
    }

    ContainerTableView::ContainerTableView(
        std::span<const ContainerReference> entries)
        :
        m_entries(entries)
    {
    }

    const ContainerReference& ContainerTableView::operator[](uint32_t index) const
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range(
                "ContainerTableView: index out of range.");
        }

        return m_entries[index];
    }

    uint32_t ContainerTableView::size() const noexcept
    {
        return static_cast<uint32_t>(
            m_entries.size());
    }

    bool ContainerTableView::empty() const noexcept
    {
        return m_entries.empty();
    }
}