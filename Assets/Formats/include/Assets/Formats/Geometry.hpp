#pragma once

#include <Assets/Formats/Common.hpp>

namespace shuttle::assets::formats::geometry
{
    enum class MeshAttributeFlags : uint32_t
    {
        None        = 0,
        HasNormal   = 1u << 0,
        HasTangent  = 1u << 1,
        HasUV       = 1u << 2,
        HasColor    = 1u << 3,
        HasSkinning = 1u << 4
    };

    inline constexpr uint32_t MaxMeshLods = 4;

    struct alignas(16) MeshLod
    {
        uint32_t firstIndex{};
        uint32_t indexCount{};

        float geometricError{};
        float reserved{};
    };

    struct alignas(16) GpuMesh
    {
        uint32_t firstVertex{};
        uint32_t vertexCount{};

        uint32_t lodCount{};
        uint32_t flags{};

        formats::AABB localBounds{};

        MeshLod lods[MaxMeshLods]{};
    };
}