#include "GeometryBuilder.hpp"

#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace shuttle::assets::scene_compiler
{
struct GeometryBuildResult;
struct ImportedMesh;

namespace
{
namespace geometry_format = shuttle::assets::formats::geometry;

[[nodiscard]] geometry_format::BoundingSphere calculateBoundingSphere(const glm::vec3& minBounds,
                                                                      const glm::vec3& maxBounds)
{
    geometry_format::BoundingSphere sphere{};

    sphere.center = (minBounds + maxBounds) * 0.5f;

    sphere.radius = glm::length(maxBounds - sphere.center);

    return sphere;
}

struct PackedVertex
{
    glm::vec3 position{0.0f};

    glm::vec3 normal{0.0f};

    glm::vec4 tangent{0.0f};

    glm::vec2 texCoord0{0.0f};

    glm::vec4 color{1.0f};

    VertexBoneInfluence skinning{};
};

struct LocalLod
{
    std::vector<uint32_t> indices;

    float screenThreshold = 0.0f;

    float geometricError = 0.0f;
};

[[nodiscard]]
bool hasSkinning(const ImportedMesh& mesh)
{
    return mesh.skinning.size() == mesh.vertices.size();
}

uint32_t buildMeshFlags(const ImportedMesh& mesh, const glm::vec3& minBounds, const glm::vec3& maxBounds)
{
    uint32_t flags = 0;

    if (hasSkinning(mesh))
    {
        flags |= static_cast<uint32_t>(geometry_format::MeshFlags::HasSkinning);
    }

    if (!mesh.morphTargets.empty())
    {
        flags |= static_cast<uint32_t>(geometry_format::MeshFlags::HasMorphing);
    }

    glm::vec3 extents = maxBounds - minBounds;

    float xy = extents.x * extents.y;

    float xz = extents.x * extents.z;

    float yz = extents.y * extents.z;

    float maxFaceArea = std::max({xy, xz, yz});

    constexpr float OccluderAreaThreshold = 0.5f;

    if (maxFaceArea >= OccluderAreaThreshold)
    {
        flags |= static_cast<uint32_t>(geometry_format::MeshFlags::Occluder);
    }

    return flags;
}

[[nodiscard]]
PackedVertex makePackedVertex(const ImportedMesh& mesh, uint32_t vertexIndex)
{
    const ImportedVertex& importedVertex = mesh.vertices[vertexIndex];

    PackedVertex result{};
    result.position = importedVertex.position;

    result.normal = importedVertex.normal;

    result.tangent = importedVertex.tangent;

    result.texCoord0 = importedVertex.texCoord0;

    result.color = importedVertex.color;

    if (hasSkinning(mesh))
    {
        result.skinning = mesh.skinning[vertexIndex];
    }

    return result;
}

[[nodiscard]]
glm::vec3 calculateMinBounds(const std::vector<PackedVertex>& vertices)
{
    glm::vec3 minBounds(std::numeric_limits<float>::max());

    for (const PackedVertex& vertex : vertices)
    {
        minBounds = glm::min(minBounds, vertex.position);
    }

    return minBounds;
}

[[nodiscard]]
glm::vec3 calculateMaxBounds(const std::vector<PackedVertex>& vertices)
{
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    for (const PackedVertex& vertex : vertices)
    {
        maxBounds = glm::max(maxBounds, vertex.position);
    }

    return maxBounds;
}

[[nodiscard]]
std::array<float, geometry_format::MaxMeshLods> makeLodRatios(const GeometryBuilderOptions& options)
{
    return {1.0f, options.lod1Ratio, options.lod2Ratio, options.lod3Ratio};
}

[[nodiscard]]
std::array<float, geometry_format::MaxMeshLods> makeLodThresholds(const GeometryBuilderOptions& options)
{
    return {options.lod0ScreenThreshold, options.lod1ScreenThreshold, options.lod2ScreenThreshold,
            options.lod3ScreenThreshold};
}

[[nodiscard]]
std::vector<uint32_t> buildRawTriangleIndices(const ImportedMesh& mesh)
{
    std::vector<uint32_t> result;
    result.reserve(mesh.indices.size());

    const size_t triangleCount = mesh.indices.size() / 3;

    for (size_t triangle = 0; triangle < triangleCount; ++triangle)
    {
        const uint32_t i0 = mesh.indices[triangle * 3 + 0];

        const uint32_t i1 = mesh.indices[triangle * 3 + 1];

        const uint32_t i2 = mesh.indices[triangle * 3 + 2];

        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
        {
            continue;
        }

        result.push_back(i0);
        result.push_back(i1);
        result.push_back(i2);
    }

    return result;
}

[[nodiscard]]
bool deduplicateMesh(const ImportedMesh& mesh, const std::vector<uint32_t>& rawIndices,
                     std::vector<PackedVertex>& outVertices, std::vector<uint32_t>& outIndices)
{
    std::vector<PackedVertex> sourceVertices;
    sourceVertices.resize(mesh.vertices.size());

    for (uint32_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex)
    {
        sourceVertices[vertexIndex] = makePackedVertex(mesh, vertexIndex);
    }

    std::vector<unsigned int> remap;
    remap.resize(sourceVertices.size());

    const size_t uniqueVertexCount =
        meshopt_generateVertexRemap(remap.data(), rawIndices.data(), rawIndices.size(), sourceVertices.data(),
                                    sourceVertices.size(), sizeof(PackedVertex));

    if (uniqueVertexCount == 0)
    {
        return false;
    }

    outVertices.resize(uniqueVertexCount);

    outIndices.resize(rawIndices.size());

    meshopt_remapVertexBuffer(outVertices.data(), sourceVertices.data(), sourceVertices.size(), sizeof(PackedVertex),
                              remap.data());

    meshopt_remapIndexBuffer(outIndices.data(), rawIndices.data(), rawIndices.size(), remap.data());

    return true;
}

void optimizeLodIndexOrder(std::vector<uint32_t>& indices, size_t vertexCount,
                           const std::vector<PackedVertex>& vertices, bool optimizeVertexCache, bool optimizeOverdraw)
{
    if (indices.empty())
    {
        return;
    }

    if (optimizeVertexCache)
    {
        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertexCount);
    }

    if (optimizeOverdraw)
    {
        meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(),
                                 reinterpret_cast<const float*>(&vertices[0].position), vertices.size(),
                                 sizeof(PackedVertex), 1.05f);
    }
}

[[nodiscard]]
std::vector<LocalLod> generateLods(const std::vector<PackedVertex>& vertices, const std::vector<uint32_t>& lod0Indices,
                                   const GeometryBuilderOptions& options)
{
    std::vector<LocalLod> lods;

    const auto ratios = makeLodRatios(options);

    const auto thresholds = makeLodThresholds(options);

    const uint32_t maxLodCount = std::min(options.maxLodCount, geometry_format::MaxMeshLods);

    LocalLod lod0{};
    lod0.indices = lod0Indices;

    lod0.screenThreshold = thresholds[0];

    lod0.geometricError = 0.0f;

    optimizeLodIndexOrder(lod0.indices, vertices.size(), vertices, options.optimizeVertexCache,
                          options.optimizeOverdraw);

    lods.push_back(std::move(lod0));

    if (!options.generateLods)
    {
        return lods;
    }

    size_t previousIndexCount = lods.front().indices.size();

    for (uint32_t lodIndex = 1; lodIndex < maxLodCount; ++lodIndex)
    {
        const size_t targetIndexCount = static_cast<size_t>(static_cast<float>(lod0Indices.size()) * ratios[lodIndex]);

        if (targetIndexCount < 3)
        {
            break;
        }

        std::vector<uint32_t> simplifiedIndices;
        simplifiedIndices.resize(lod0Indices.size());

        float resultError = 0.0f;

        const size_t simplifiedIndexCount =
            meshopt_simplify(simplifiedIndices.data(), lod0Indices.data(), lod0Indices.size(),
                             reinterpret_cast<const float*>(&vertices[0].position), vertices.size(),
                             sizeof(PackedVertex), targetIndexCount, options.simplifyTargetError, 0, &resultError);

        if (simplifiedIndexCount < 3)
        {
            break;
        }

        if (simplifiedIndexCount >= previousIndexCount)
        {
            break;
        }

        simplifiedIndices.resize(simplifiedIndexCount);

        optimizeLodIndexOrder(simplifiedIndices, vertices.size(), vertices, options.optimizeVertexCache, false);

        LocalLod lod{};
        lod.indices = std::move(simplifiedIndices);

        lod.screenThreshold = thresholds[lodIndex];

        lod.geometricError = resultError;

        previousIndexCount = lod.indices.size();

        lods.push_back(std::move(lod));
    }

    return lods;
}

void concatenateLodIndices(const std::vector<LocalLod>& lods, std::vector<uint32_t>& outIndices,
                           std::array<uint32_t, geometry_format::MaxMeshLods>& firstIndices,
                           std::array<uint32_t, geometry_format::MaxMeshLods>& indexCounts)
{
    outIndices.clear();

    for (uint32_t lodIndex = 0; lodIndex < lods.size(); ++lodIndex)
    {
        firstIndices[lodIndex] = static_cast<uint32_t>(outIndices.size());

        indexCounts[lodIndex] = static_cast<uint32_t>(lods[lodIndex].indices.size());

        outIndices.insert(outIndices.end(), lods[lodIndex].indices.begin(), lods[lodIndex].indices.end());
    }
}

void optimizeVertexFetchForAllLods(std::vector<PackedVertex>& vertices, std::vector<uint32_t>& combinedIndices,
                                   bool enabled)
{
    if (!enabled || vertices.empty() || combinedIndices.empty())
    {
        return;
    }

    std::vector<PackedVertex> optimizedVertices;
    optimizedVertices.resize(vertices.size());

    const size_t optimizedVertexCount =
        meshopt_optimizeVertexFetch(optimizedVertices.data(), combinedIndices.data(), combinedIndices.size(),
                                    vertices.data(), vertices.size(), sizeof(PackedVertex));

    optimizedVertices.resize(optimizedVertexCount);

    vertices = std::move(optimizedVertices);
}

formats::PositionAttribute makePositionAttribute(const PackedVertex& vertex)
{
    formats::PositionAttribute result{};
    result.position = glm::vec4(vertex.position, 1.0f);

    return result;
}

formats::VertexAttribute makeVertexAttribute(const PackedVertex& vertex)
{
    formats::VertexAttribute result{};

    result.normal = glm::vec4(vertex.normal, 0.0f);

    result.tangent = vertex.tangent;

    result.uv = glm::vec4(vertex.texCoord0, 0.0f, 0.0f);

    return result;
}

formats::VertexSkin makeVertexSkin(const PackedVertex& vertex)
{
    formats::VertexSkin result{};

    result.boneIndices = glm::uvec4(vertex.skinning.boneIndices[0], vertex.skinning.boneIndices[1],
                                    vertex.skinning.boneIndices[2], vertex.skinning.boneIndices[3]);

    result.boneWeights = glm::vec4(vertex.skinning.boneWeights[0], vertex.skinning.boneWeights[1],
                                   vertex.skinning.boneWeights[2], vertex.skinning.boneWeights[3]);

    return result;
}

void appendVertices(GeometryBuildResult& result, const std::vector<PackedVertex>& vertices)
{
    for (const PackedVertex& vertex : vertices)
    {
        result.positions.push_back(makePositionAttribute(vertex));

        result.attributes.push_back(makeVertexAttribute(vertex));

        result.skins.push_back(makeVertexSkin(vertex));
    }
}

void appendIndices(GeometryBuildResult& result, const std::vector<uint32_t>& localIndices)
{
    result.indices.reserve(result.indices.size() + localIndices.size());

    for (uint32_t index : localIndices)
    {
        result.indices.push_back(index);
    }
}

[[nodiscard]]
geometry_format::GpuMesh buildGpuMesh(const ImportedMesh& importedMesh, const std::vector<PackedVertex>& vertices,
                                      const std::vector<LocalLod>& lods,
                                      const std::array<unsigned, 4>& globalFirstIndices, uint32_t firstVertex)
{
    geometry_format::GpuMesh gpuMesh{};

    gpuMesh.positionOffset = firstVertex;

    gpuMesh.attributeOffset = firstVertex;

    gpuMesh.lodCount = static_cast<uint32_t>(lods.size());

    const glm::vec3 minBounds = calculateMinBounds(vertices);

    const glm::vec3 maxBounds = calculateMaxBounds(vertices);

    gpuMesh.meshFlags = buildMeshFlags(importedMesh, minBounds, maxBounds);

    gpuMesh.localBounds.min = glm::vec4(minBounds, 1.0f);

    gpuMesh.localBounds.max = glm::vec4(maxBounds, 1.0f);

    gpuMesh.boundingSphere = calculateBoundingSphere(minBounds, maxBounds);

    for (uint32_t lodIndex = 0; lodIndex < lods.size(); ++lodIndex)
    {
        gpuMesh.lods[lodIndex].firstIndex = globalFirstIndices[lodIndex];

        gpuMesh.lods[lodIndex].indexCount = static_cast<uint32_t>(lods[lodIndex].indices.size());

        gpuMesh.lods[lodIndex].geometricError = lods[lodIndex].geometricError;

        gpuMesh.lods[lodIndex].screenThreshold = lods[lodIndex].screenThreshold;
    }

    return gpuMesh;
}

bool buildSingleMesh(const ImportedMesh& importedMesh, GeometryBuildResult& result,
                     const GeometryBuilderOptions& options, int32_t& outCompiledIndex)
{
    outCompiledIndex = InvalidIndexI32;

    if (importedMesh.vertices.empty() || importedMesh.indices.empty())
    {
        return false;
    }

    const std::vector<uint32_t> rawIndices = buildRawTriangleIndices(importedMesh);

    if (rawIndices.empty())
    {
        return false;
    }

    std::vector<PackedVertex> uniqueVertices;
    std::vector<uint32_t> uniqueIndices;

    if (!deduplicateMesh(importedMesh, rawIndices, uniqueVertices, uniqueIndices))
    {
        return false;
    }

    std::vector<LocalLod> lods = generateLods(uniqueVertices, uniqueIndices, options);

    if (lods.empty())
    {
        return false;
    }

    std::vector<uint32_t> combinedIndices;

    std::array<uint32_t, geometry_format::MaxMeshLods> localFirstIndices{};
    std::array<uint32_t, geometry_format::MaxMeshLods> localIndexCounts{};

    concatenateLodIndices(lods, combinedIndices, localFirstIndices, localIndexCounts);

    optimizeVertexFetchForAllLods(uniqueVertices, combinedIndices, options.optimizeVertexFetch);

    const uint32_t firstVertex = static_cast<uint32_t>(result.positions.size());

    const uint32_t vertexCount = static_cast<uint32_t>(uniqueVertices.size());

    std::array<uint32_t, geometry_format::MaxMeshLods> globalFirstIndices{};

    uint32_t globalIndexCursor = static_cast<uint32_t>(result.indices.size());

    for (uint32_t lodIndex = 0; lodIndex < lods.size(); ++lodIndex)
    {
        globalFirstIndices[lodIndex] = globalIndexCursor + localFirstIndices[lodIndex];
    }

    appendVertices(result, uniqueVertices);

    appendIndices(result, combinedIndices);

    geometry_format::GpuMesh gpuMesh =
        buildGpuMesh(importedMesh, uniqueVertices, lods, globalFirstIndices, firstVertex);

    outCompiledIndex = static_cast<int32_t>(result.meshes.size());

    result.meshes.push_back(gpuMesh);

    return true;
}
} // namespace

GeometryBuildResult GeometryBuilder::build(const ImportedScene& scene, const GeometryBuilderOptions& options)
{
    GeometryBuildResult result{};

    result.importedToCompiledMesh.resize(scene.meshes.size(), InvalidIndexI32);

    result.meshes.reserve(scene.meshes.size());

    for (size_t meshIndex = 0; meshIndex < scene.meshes.size(); ++meshIndex)
    {
        int32_t compiledMeshIndex = InvalidIndexI32;

        const bool built = buildSingleMesh(scene.meshes[meshIndex], result, options, compiledMeshIndex);

        if (!built)
        {
            continue;
        }

        result.importedToCompiledMesh[meshIndex] = compiledMeshIndex;
    }

    return result;
}
} // namespace shuttle::assets::scene_compiler