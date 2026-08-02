#pragma once

#include <cstdint>

namespace shuttle::assets::core
{
inline constexpr char BlobMagic[4] = {'S', 'B', 'L', 'B'};
inline constexpr uint32_t InvalidContainerIndex = 0xFFFFFFFF;
inline constexpr uint32_t InvalidStringOffset = 0xFFFFFFFF;
inline constexpr uint32_t BlobFormatVersion = 1;

enum class BlobSectionType : uint32_t
{
    Unknown = 0,

    // Generic
    StringTable,

    // CPU scene/editor data
    CpuSceneNodes = 1000,
    CpuNodeHierarchy,
    CpuMeshMetadata,
    CpuMaterialMetadata,
    CpuLightMetadata,
    CpuInspectorData,

    // GPU scene/runtime data
    GpuMaterials = 2000,
    GpuSceneTransforms,
    GpuSceneNodes,
    GpuNodeLevels,
    GpuMeshes,
    GpuDrawableObjects,

    GpuDirectionalLights = 3000,
    GpuPointLights,
    GpuSpotLights,

    // Geometry megabuffers
    PositionMegabuffer = 4000,
    AttributeMegabuffer,
    SkinMegabuffer,
    IndexMegabuffer,

    // Animation
    Skeletons = 5000,
    Bones,
    AnimationClips,
    TransformChannels,
    MorphChannels,
    MaterialChannels,
    KeyframeTimes,
    KeyframeValues,
    MorphTargets,
    MorphVertexDeltas,
    MaterialProperties,

    // Textures
    TextureMetadata = 6000,
    TextureMipMetadata,
    TextureData,

    // Environment
    EnvironmentInfo = 7000,
    EnvironmentTextureMetadata,
    EnvironmentTextureMipMetadata,
    EnvironmentTextureData,

    // Future
    Meshlets = 8000,
    RayTracingData,

    Custom
};

enum class BlobSectionFlags : uint32_t
{
    None = 0,
    Compressed = 1u << 0,
    Optional = 1u << 1,
    External = 1u << 2
};

[[nodiscard]] inline uint32_t toRaw(BlobSectionFlags flags) noexcept
{
    return static_cast<uint32_t>(flags);
}

[[nodiscard]] inline bool hasFlag(uint32_t flags, BlobSectionFlags flag) noexcept
{
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

struct alignas(16) BlobHeader
{
    char magic[4]{};
    uint32_t version{};

    uint64_t totalFileSize{};

    uint64_t containerTableOffset{};
    uint32_t containerCount{};
    uint32_t padding0{};

    uint64_t sectionTableOffset{};
    uint32_t sectionCount{};
    uint32_t padding1{};

    uint64_t stringTableOffset{};
    uint64_t stringTableSize{};

    uint64_t reserved0{};
    uint64_t reserved1{};
};

static_assert(sizeof(BlobHeader) == 80);
static_assert(sizeof(BlobHeader) % 16 == 0);

struct alignas(16) BlobSection
{
    BlobSectionType type{BlobSectionType::Unknown};
    uint32_t flags{};

    // 0 = current file.
    // Other values reference ContainerReference table.
    uint32_t containerIndex{};
    uint32_t reserved0{};

    // Absolute offset inside the target container.
    uint64_t offset{};

    uint64_t size{};
};

static_assert(sizeof(BlobSection) == 32);
static_assert(sizeof(BlobSection) % 16 == 0);

struct alignas(16) ContainerReference
{
    // Offset into global string table.
    // Container index 0 is always "self".
    uint32_t pathStringOffset{};
    uint32_t flags{};

    uint64_t guidLow{};
    uint64_t guidHigh{};

    uint64_t reserved0{};
};

static_assert(sizeof(ContainerReference) == 32);
static_assert(sizeof(ContainerReference) % 16 == 0);

[[nodiscard]] inline uint64_t align16(uint64_t value) noexcept
{
    return (value + 15ull) & ~15ull;
}
} // namespace shuttle::assets::core