#pragma once

#include <Assets/Formats/Environment.hpp>
#include <Assets/TextureCompiler/CompiledTexture.hpp>

#include <vector>

namespace shuttle::assets::environment_compiler
{
struct CompiledEnvironment
{
    formats::environment::EnvironmentInfo info{};

    std::vector<texture_compiler::CompiledTexture> textures;
};
} // namespace shuttle::assets::environment_compiler