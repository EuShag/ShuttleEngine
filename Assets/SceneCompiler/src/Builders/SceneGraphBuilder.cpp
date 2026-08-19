#include "SceneGraphBuilder.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <queue>

#include "Assets/Formats/Common.hpp"
#include "Assets/Formats/Scene.hpp"
#include "Intermediate/ImportedScene.hpp"

namespace shuttle::assets::scene_compiler
{
namespace
{
uint32_t fnv1aHash(const std::string& text)
{
    constexpr uint32_t basis = 2166136261u;

    constexpr uint32_t prime = 16777619u;

    uint32_t hash = basis;

    for (unsigned char c : text)
    {
        hash ^= c;
        hash *= prime;
    }

    return hash;
}

void decomposeTransform(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
{
    glm::vec3 skew;
    glm::vec4 perspective;

    glm::decompose(matrix, scale, rotation, translation, skew, perspective);

    rotation = glm::normalize(rotation);
}

uint32_t buildNodeFlags(const ImportedNode& node)
{
    uint32_t flags = 0;

    if (!node.meshes.empty())
    {
        flags |= 1u << 0;
    }

    if (node.skinIndex >= 0)
    {
        flags |= 1u << 1;
    }

    if (node.lightIndex >= 0)
    {
        flags |= 1u << 2;
    }

    return flags;
}

uint32_t buildDrawableFlags(const ImportedNode& node)
{
    uint32_t flags = 0;

    if (node.skinIndex >= 0)
    {
        flags |= 1u << 0;
    }

    return flags;
}
} // namespace

SceneGraphBuildResult SceneGraphBuilder::build(const ImportedScene& scene, const GeometryBuildResult& geometry,
                                               const MaterialBuildResult& materials)
{
    SceneGraphBuildResult result{};

    result.nodes.reserve(scene.nodes.size());

    result.transforms.reserve(scene.nodes.size());

    result.importedNodeToRuntimeNode.resize(scene.nodes.size());

    // ==========================================================
    // pass 1 : build nodes + transforms
    // ==========================================================

    for (size_t nodeIndex = 0; nodeIndex < scene.nodes.size(); ++nodeIndex)
    {
        const ImportedNode& imported = scene.nodes[nodeIndex];

        formats::scene::SceneNode runtimeNode{};

        formats::scene::Transform transform{};

        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;

        decomposeTransform(imported.localTransform, translation, rotation, scale);

        transform.translation = glm::vec4{translation, 0.0f};

        transform.rotationQuat = glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);

        transform.scale = glm::vec4{scale, 0.0f};

        const auto transformIndex = static_cast<uint32_t>(result.transforms.size());

        result.transforms.push_back(transform);

        runtimeNode.parentIndex =
            imported.parent >= 0 ? static_cast<uint32_t>(imported.parent) : formats::InvalidIndexU32;

        runtimeNode.transformIndex = transformIndex;

        runtimeNode.animationBindingIndex = formats::InvalidIndexU32;

        runtimeNode.nodeNameHash = fnv1aHash(imported.name);

        result.importedNodeToRuntimeNode[nodeIndex] = static_cast<int32_t>(result.nodes.size());

        result.nodes.push_back(runtimeNode);
    }

    // ==========================================================
    // pass 2 : create drawable objects
    // ==========================================================

    for (uint32_t nodeIndex = 0; nodeIndex < scene.nodes.size(); ++nodeIndex)
    {
        const ImportedNode& importedNode = scene.nodes[nodeIndex];

        const uint32_t transformIndex = result.nodes[nodeIndex].transformIndex;

        for (int32_t importedMesh : importedNode.meshes)
        {
            if (importedMesh < 0)
            {
                continue;
            }

            if (importedMesh >= static_cast<int32_t>(geometry.importedToCompiledMesh.size()))
            {
                continue;
            }

            const int32_t runtimeMesh = geometry.importedToCompiledMesh[importedMesh];

            if (runtimeMesh < 0)
            {
                continue;
            }

            formats::scene::GpuDrawableObject drawable{};

            drawable.transformIndex = transformIndex;

            drawable.meshIndex = static_cast<uint32_t>(runtimeMesh);

            const ImportedMesh& mesh = scene.meshes[static_cast<size_t>(importedMesh)];

            if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int32_t>(materials.importedToCompiledMaterial.size()))
            {
                const int32_t compiledMaterialIndex = materials.importedToCompiledMaterial[mesh.materialIndex];

                if (compiledMaterialIndex >= 0)
                {
                    drawable.materialIndex = static_cast<uint32_t>(compiledMaterialIndex);
                }
                else
                {
                    drawable.materialIndex = formats::InvalidIndexU32;
                }
            }
            else
            {
                drawable.materialIndex = formats::InvalidIndexU32;
            }

            drawable.flags = buildDrawableFlags(importedNode);

            result.drawableObjects.push_back(drawable);
        }
    }

    // ==========================================================
    // pass 3 : build BFS node levels
    // ==========================================================

    std::queue<uint32_t> currentLevel;
    std::queue<uint32_t> nextLevel;

    for (uint32_t i = 0; i < scene.nodes.size(); ++i)
    {
        if (scene.nodes[i].parent < 0)
        {
            currentLevel.push(i);
        }
    }

    while (!currentLevel.empty())
    {
        formats::scene::NodeLevelRange level{};

        level.startIndex = currentLevel.front();

        uint32_t nodeCount = 0;

        while (!currentLevel.empty())
        {
            const uint32_t node = currentLevel.front();

            currentLevel.pop();

            ++nodeCount;

            for (int32_t child : scene.nodes[node].children)
            {
                if (child >= 0)
                {
                    nextLevel.push(static_cast<uint32_t>(child));
                }
            }
        }

        level.count = nodeCount;

        result.levels.push_back(level);

        std::swap(currentLevel, nextLevel);
    }

    return result;
}
} // namespace shuttle::assets::scene_compiler
