#pragma once

#include <Assets/EnvironmentCompiler/CompiledEnvironment.hpp>

#include <filesystem>
#include <optional>

#include "../include/Assets/EnvironmentCompiler/CpuIblGenerator.hpp"

namespace shuttle::assets::environment_compiler
{
class EnvironmentBaker
{
  public:
    [[nodiscard]]
    static std::optional<CompiledEnvironment> bake(const std::filesystem::path &hdrFile, engine::ibl::IblGenerationSettings const &settings);
};
} // namespace shuttle::assets::environment_compiler