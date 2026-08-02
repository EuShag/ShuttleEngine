#pragma once

#include <Assets/EnvironmentCompiler/CompiledEnvironment.hpp>

#include <filesystem>

namespace shuttle::assets::environment_compiler
{
class CompiledEnvironmentBlobWriter
{
  public:
    [[nodiscard]]
    static bool write(const CompiledEnvironment& environment, const std::filesystem::path& outputPath);
};
} // namespace shuttle::assets::environment_compiler