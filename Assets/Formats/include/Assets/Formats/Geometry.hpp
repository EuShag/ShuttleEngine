#pragma once

#include <Assets/Formats/Common.hpp>

namespace shuttle::assets::formats::geometry
{
enum class MeshFlags : uint32_t
{
    None = 0,

    HasSkinning = 1u << 0,
    HasMorphing = 1u << 1,

    Occluder = 1u << 2,
    Transparent = 1u << 3,
    DoubleSided = 1u << 4,
    ShadowCaster = 1u << 5
};

inline constexpr uint32_t MaxMeshLods = 4;

struct alignas(16) BoundingSphere
{
    glm::vec3 center{};
    float radius{};
};

static_assert(sizeof(BoundingSphere) == 16);

struct alignas(16) MeshLod
{
    uint32_t firstIndex{};
    uint32_t indexCount{};

    float geometricError{};

    // Минимальный экранный размер для использования этого LOD
    // (в пикселях или нормализованная метрика экрана).
    float screenThreshold{};
};

static_assert(sizeof(MeshLod) == 16);

struct alignas(16) GpuMesh
{
    uint32_t positionOffset{};
    uint32_t attributeOffset{};

    uint32_t lodCount{};
    uint32_t meshFlags{};

    // Быстрый culling и LOD.
    BoundingSphere boundingSphere{};

    // Точный culling.
    AABB localBounds{};

    MeshLod lods[MaxMeshLods]{};
};
} // namespace shuttle::assets::formats::geometry