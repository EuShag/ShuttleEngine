#pragma once

#include <filesystem>

#include "EnvironmentFormat.hpp"
#include "../EnvironmentBaker/EnvironmentBaker.hpp"

namespace shuttle_engine::assets
{
    class EnvironmentBlobWriter
    {
    public:

        static bool write(
            const EnvironmentBakeResult& environment,
            const std::filesystem::path& outputPath);
    };
}
