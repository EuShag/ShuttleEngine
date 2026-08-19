#include <Assets/EnvironmentCompiler/EnvironmentCompiler.hpp>

#include "EnvironmentBaker.hpp"

namespace shuttle::assets::environment_compiler
{
std::optional<CompiledEnvironment> EnvironmentCompiler::compile(const std::filesystem::path& hdrFile, engine::ibl::IblGenerationSettings const &settings)
{
    return EnvironmentBaker::bake(hdrFile, settings);
}
} // namespace shuttle::assets::environment_compiler