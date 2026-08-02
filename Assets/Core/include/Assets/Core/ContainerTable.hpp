#pragma once

#include <Assets/Core/BlobFormat.hpp>
#include <Assets/Core/StringTable.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace shuttle::assets::core
{
class ContainerTableBuilder
{
  public:
    ContainerTableBuilder() = default;

    [[nodiscard]] uint32_t addSelf();

    [[nodiscard]] uint32_t addExternalContainer(StringTableBuilder& strings, std::string_view path,
                                                uint64_t guidLow = 0, uint64_t guidHigh = 0, uint32_t flags = 0);

    [[nodiscard]] const std::vector<ContainerReference>& entries() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear();

  private:
    std::vector<ContainerReference> m_entries;
};

class ContainerTableView
{
  public:
    ContainerTableView() = default;

    explicit ContainerTableView(std::span<const ContainerReference> entries);

    [[nodiscard]] const ContainerReference& operator[](uint32_t index) const;
    [[nodiscard]] uint32_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

  private:
    std::span<const ContainerReference> m_entries;
};
} // namespace shuttle::assets::core