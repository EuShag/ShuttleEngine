#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shuttle::assets::core
{
class StringTableBuilder
{
  public:
    StringTableBuilder();

    [[nodiscard]] uint32_t addString(std::string_view value);
    [[nodiscard]] const std::vector<uint8_t>& bytes() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear();

  private:
    std::vector<uint8_t> m_bytes;
    std::unordered_map<std::string, uint32_t> m_offsets;
};

class StringTableView
{
  public:
    StringTableView() = default;
    StringTableView(const uint8_t* data, uint64_t size);

    [[nodiscard]] const char* c_str(uint32_t offset) const;
    [[nodiscard]] bool validOffset(uint32_t offset) const noexcept;
    [[nodiscard]] const uint8_t* data() const noexcept;
    [[nodiscard]] uint64_t size() const noexcept;

  private:
    const uint8_t* m_data = nullptr;
    uint64_t m_size = 0;
};
} // namespace shuttle::assets::core