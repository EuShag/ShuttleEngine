#pragma once

#include <Assets/Core/BlobView.hpp>

#include <filesystem>

namespace shuttle::assets::core
{
class BlobReader
{
  public:
    [[nodiscard]] static BlobView open(const std::filesystem::path& path);
};
} // namespace shuttle::assets::core