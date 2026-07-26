#pragma once

#include "../Intermediate/ImportedScene.hpp"

#include <Assets/Formats/Lighting.hpp>

#include <string>
#include <vector>

namespace shuttle::assets::scene_compiler
{
    struct LightingBuildResult
    {
        bool success = true;

        std::string errorMessage;

        std::vector<formats::lighting::DirectionalLight>
            directionalLights;

        std::vector<formats::lighting::PointLight>
            pointLights;

        std::vector<formats::lighting::SpotLight>
            spotLights;
    };

    class LightingBuilder
    {
    public:
        [[nodiscard]]
        static LightingBuildResult build(
            const ImportedScene& scene);
    };
}