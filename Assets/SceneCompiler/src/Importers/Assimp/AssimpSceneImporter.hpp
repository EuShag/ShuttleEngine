#pragma once

#include "../../Intermediate/ImportedScene.hpp"

#include <filesystem>
#include <optional>

namespace shuttle::assets::scene_compiler
{
    class AssimpSceneImporter
    {
    public:
        [[nodiscard]]
        static std::optional<ImportedScene> import(
            const std::filesystem::path& filePath);
    };
}