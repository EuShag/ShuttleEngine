#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace shuttle::assets::formats::material
{
constexpr uint32_t InvalidTextureIndex = std::numeric_limits<uint32_t>::max();

enum class AlphaMode : uint32_t
{
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

enum class MaterialFlags : uint32_t
{
    None = 0,

    HasAlbedoMap = 1u << 0,
    HasNormalMap = 1u << 1,
    HasORMMap = 1u << 2,
    HasEmissiveMap = 1u << 3,

    AlphaMask = 1u << 4,
    AlphaBlend = 1u << 5,

    DoubleSided = 1u << 6,

    Emissive = 1u << 7
};

[[nodiscard]]
constexpr MaterialFlags operator|(MaterialFlags lhs, MaterialFlags rhs) noexcept
{
    return static_cast<MaterialFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

[[nodiscard]]
constexpr MaterialFlags operator&(MaterialFlags lhs, MaterialFlags rhs) noexcept
{
    return static_cast<MaterialFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

[[nodiscard]]
constexpr MaterialFlags operator^(MaterialFlags lhs, MaterialFlags rhs) noexcept
{
    return static_cast<MaterialFlags>(static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs));
}

[[nodiscard]]
constexpr MaterialFlags operator~(MaterialFlags value) noexcept
{
    return static_cast<MaterialFlags>(~static_cast<uint32_t>(value));
}

constexpr MaterialFlags& operator|=(MaterialFlags& lhs, MaterialFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr MaterialFlags& operator&=(MaterialFlags& lhs, MaterialFlags rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

constexpr MaterialFlags& operator^=(MaterialFlags& lhs, MaterialFlags rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

[[nodiscard]]
constexpr bool hasFlag(MaterialFlags value, MaterialFlags flag) noexcept
{
    return (value & flag) != MaterialFlags::None;
}

struct alignas(16) MaterialInfo
{
    glm::vec4 baseColorFactor{1.0f};
    glm::vec4 emissiveFactor{0.0f};

    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float alphaCutoff = 0.5f;
    float occlusionStrength = 1.0f;

    float emissiveStrength = 1.0f;

    uint32_t flags = 0;
    uint32_t pipelineFlags = 0;
    AlphaMode alphaMode = AlphaMode::Opaque;

    uint32_t albedoTexture = InvalidTextureIndex;
    uint32_t normalTexture = InvalidTextureIndex;
    uint32_t ormTexture = InvalidTextureIndex;
    uint32_t emissiveTexture = InvalidTextureIndex;

    uint32_t reserved0 = 0;
};

static_assert(sizeof(MaterialInfo) == 96);
} // namespace shuttle::assets::formats::material