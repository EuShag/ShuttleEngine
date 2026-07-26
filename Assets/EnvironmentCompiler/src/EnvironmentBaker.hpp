#pragma once

#include <Assets/EnvironmentCompiler/CompiledEnvironment.hpp>

#include <cmft/allocator.h>
#include <cmft/image.h>

#include <filesystem>
#include <optional>

namespace shuttle::assets::environment_compiler
{
    class EnvironmentBaker
    {
    public:
        [[nodiscard]]
        static std::optional<CompiledEnvironment> bake(
            const std::filesystem::path& hdrFile);

    private:
        [[nodiscard]]
        static cmft::CrtAllocator* allocator()
        {
            return &cmft::g_crtAllocator;
        }
    };
}