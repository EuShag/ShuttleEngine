#include <Assets/EnvironmentCompiler/EnvironmentCompiler.hpp>

#include "EnvironmentBaker.hpp"

namespace shuttle::assets::environment_compiler
{
std::optional<CompiledEnvironment> EnvironmentCompiler::compile(const std::filesystem::path& hdrFile)
{
    return EnvironmentBaker::bake(hdrFile);
}
} // namespace shuttle::assets::environment_compiler