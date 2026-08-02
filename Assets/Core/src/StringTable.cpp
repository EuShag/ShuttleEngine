#include <Assets/Core/StringTable.hpp>

#include <stdexcept>

namespace shuttle::assets::core
{
StringTableBuilder::StringTableBuilder()
{
    clear();
}

uint32_t StringTableBuilder::addString(std::string_view value)
{
    const std::string key(value);

    if (const auto it = m_offsets.find(key); it != m_offsets.end())
    {
        return it->second;
    }
    const auto offset = static_cast<uint32_t>(m_bytes.size());

    m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    m_bytes.push_back(0);
    m_offsets.emplace(key, offset);

    return offset;
}

const std::vector<uint8_t>& StringTableBuilder::bytes() const noexcept
{
    return m_bytes;
}

bool StringTableBuilder::empty() const noexcept
{
    return m_bytes.empty();
}

void StringTableBuilder::clear()
{
    m_bytes.clear();
    m_offsets.clear();

    // Offset 0 is always empty string.
    m_bytes.push_back(0);
    m_offsets.emplace("", 0);
}

StringTableView::StringTableView(const uint8_t* data, uint64_t size) : m_data(data), m_size(size)
{
}

const char* StringTableView::c_str(uint32_t offset) const
{
    if (!validOffset(offset)) throw std::runtime_error("StringTableView: invalid string offset.");

    return reinterpret_cast<const char*>(m_data + offset);
}

bool StringTableView::validOffset(uint32_t offset) const noexcept
{
    return m_data != nullptr && offset < m_size;
}

const uint8_t* StringTableView::data() const noexcept
{
    return m_data;
}

uint64_t StringTableView::size() const noexcept
{
    return m_size;
}
} // namespace shuttle::assets::core