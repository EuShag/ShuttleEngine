#pragma once

#include "../Runtime/CompiledScene.hpp"

#include <Assets/Core/BlobWriter.hpp>

#include <filesystem>

namespace shuttle::assets::scene_compiler
{
class CompiledSceneBlobWriter
{
  public:
    [[nodiscard]]
    static bool write(const CompiledScene& scene, const std::filesystem::path& outputPath);
};
} // namespace shuttle::assets::scene_compiler