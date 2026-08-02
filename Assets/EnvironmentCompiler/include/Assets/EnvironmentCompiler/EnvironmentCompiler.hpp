#pragma once

#include <Assets/EnvironmentCompiler/CompiledEnvironment.hpp>

#include <filesystem>
#include <optional>

namespace shuttle::assets::environment_compiler
{
class EnvironmentCompiler
{
  public:
    [[nodiscard]]
    static std::optional<CompiledEnvironment> compile(const std::filesystem::path& hdrFile);
};
} // namespace shuttle::assets::environment_compiler