#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace shuttle::assets::formats
{
inline constexpr uint32_t InvalidIndexU32 = 0xFFFFFFFFu;
inline constexpr int32_t InvalidIndexI32 = -1;

inline constexpr uint32_t InvalidHash = 0u;
inline constexpr uint64_t InvalidOffset = 0ull;

struct alignas(16) AABB
{
    glm::vec4 min;
    glm::vec4 max;
};

struct alignas(16) PositionAttribute
{
    glm::vec4 position;
};

struct alignas(16) VertexAttribute
{
    glm::vec4 normal;
    glm::vec4 tangent;
    glm::vec4 uv;
};

struct alignas(16) VertexSkin
{
    glm::uvec4 boneIndices;
    glm::vec4 boneWeights;
};
} // namespace shuttle::assets::formats