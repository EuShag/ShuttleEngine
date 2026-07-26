#include "LightingBuilder.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <vector>

namespace shuttle::assets::scene_compiler
{
    namespace
    {
        glm::mat4 calculateWorldTransform(
            const ImportedScene& scene,
            uint32_t nodeIndex,
            std::vector<uint8_t>& visited,
            std::vector<glm::mat4>& worldTransforms)
        {
            if (nodeIndex >= scene.nodes.size())
            {
                return glm::mat4(1.0f);
            }

            if (visited[nodeIndex])
            {
                return worldTransforms[nodeIndex];
            }

            const ImportedNode& node =
                scene.nodes[nodeIndex];

            glm::mat4 parentWorld{1.0f};

            if (node.parent >= 0 &&
                node.parent < static_cast<int32_t>(scene.nodes.size()))
            {
                parentWorld =
                    calculateWorldTransform(
                        scene,
                        static_cast<uint32_t>(node.parent),
                        visited,
                        worldTransforms);
            }

            worldTransforms[nodeIndex] =
                parentWorld *
                node.localTransform;

            visited[nodeIndex] =
                1;

            return worldTransforms[nodeIndex];
        }

        std::vector<glm::mat4> buildWorldTransforms(
            const ImportedScene& scene)
        {
            std::vector<glm::mat4> worldTransforms;
            worldTransforms.resize(
                scene.nodes.size(),
                glm::mat4(1.0f));

            std::vector<uint8_t> visited;
            visited.resize(
                scene.nodes.size(),
                0);

            for (uint32_t nodeIndex = 0;
                 nodeIndex < scene.nodes.size();
                 ++nodeIndex)
            {
                calculateWorldTransform(
                    scene,
                    nodeIndex,
                    visited,
                    worldTransforms);
            }

            return worldTransforms;
        }

        int32_t findNodeForLight(
            const ImportedScene& scene,
            int32_t lightIndex)
        {
            for (size_t nodeIndex = 0;
                 nodeIndex < scene.nodes.size();
                 ++nodeIndex)
            {
                if (scene.nodes[nodeIndex].lightIndex ==
                    lightIndex)
                {
                    return static_cast<int32_t>(
                        nodeIndex);
                }
            }

            return InvalidIndexI32;
        }

        glm::vec3 extractTranslation(
            const glm::mat4& transform)
        {
            return glm::vec3(
                transform[3]);
        }

        glm::vec3 extractForwardDirection(
            const glm::mat4& transform)
        {
            const glm::vec3 localForward =
                glm::vec3(0.0f, 0.0f, -1.0f);

            glm::vec3 direction =
                glm::mat3(transform) *
                localForward;

            const float length =
                glm::length(direction);

            if (length <= 0.000001f)
            {
                return glm::vec3(
                    0.0f,
                    -1.0f,
                    0.0f);
            }

            return glm::normalize(
                direction);
        }

        float safeRange(
            float value,
            float fallback)
        {
            return value > 0.0f
                ? value
                : fallback;
        }
    }

    LightingBuildResult LightingBuilder::build(
        const ImportedScene& scene)
    {
        LightingBuildResult result{};

        const std::vector<glm::mat4> worldTransforms =
            buildWorldTransforms(
                scene);

        result.directionalLights.reserve(
            scene.lights.size());

        result.pointLights.reserve(
            scene.lights.size());

        result.spotLights.reserve(
            scene.lights.size());

        for (size_t lightIndex = 0;
             lightIndex < scene.lights.size();
             ++lightIndex)
        {
            const ImportedLight& imported =
                scene.lights[lightIndex];

            const int32_t nodeIndex =
                findNodeForLight(
                    scene,
                    static_cast<int32_t>(lightIndex));

            glm::mat4 worldTransform{1.0f};

            if (nodeIndex >= 0 &&
                nodeIndex < static_cast<int32_t>(worldTransforms.size()))
            {
                worldTransform =
                    worldTransforms[static_cast<size_t>(nodeIndex)];
            }

            const glm::vec3 position =
                extractTranslation(
                    worldTransform);

            const glm::vec3 direction =
                extractForwardDirection(
                    worldTransform);

            switch (imported.type)
            {
                case ImportedLightType::Directional:
                {
                    formats::lighting::DirectionalLight light{};

                    light.directionAndIntensity =
                        glm::vec4(
                            direction,
                            imported.intensity);

                    light.color =
                        imported.color;

                    light.castShadows =
                        1;

                    result.directionalLights.push_back(
                        light);

                    break;
                }

                case ImportedLightType::Point:
                {
                    formats::lighting::PointLight light{};

                    light.positionAndRadius =
                        glm::vec4(
                            position,
                            safeRange(
                                imported.range,
                                10.0f));

                    light.color =
                        imported.color;

                    light.intensity =
                        imported.intensity;

                    result.pointLights.push_back(
                        light);

                    break;
                }

                case ImportedLightType::Spot:
                {
                    formats::lighting::SpotLight light{};

                    light.positionAndRadius =
                        glm::vec4(
                            position,
                            safeRange(
                                imported.range,
                                15.0f));

                    light.directionAndIntensity =
                        glm::vec4(
                            direction,
                            imported.intensity);

                    light.color =
                        imported.color;

                    light.innerCutoffCos =
                        std::cos(
                            imported.innerConeAngle);

                    light.outerCutoffCos =
                        std::cos(
                            imported.outerConeAngle);

                    light.castShadows =
                        0;

                    result.spotLights.push_back(
                        light);

                    break;
                }
            }
        }

        return result;
    }
}