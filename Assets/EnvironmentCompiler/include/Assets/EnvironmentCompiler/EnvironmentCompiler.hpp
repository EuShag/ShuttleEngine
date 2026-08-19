#pragma once

#include <Assets/EnvironmentCompiler/CompiledEnvironment.hpp>

#include <filesystem>
#include <optional>

#include "CpuIblGenerator.hpp"

namespace shuttle::assets::environment_compiler
{
class EnvironmentCompiler
{
  public:
    [[nodiscard]]
    static std::optional<CompiledEnvironment> compile(const std::filesystem::path &hdrFile, engine::ibl::IblGenerationSettings const &settings);
};
} // namespace shuttle::assets::environment_compiler