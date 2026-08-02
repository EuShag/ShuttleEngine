//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_TERRAIN_HPP
#define HELLOTRIANGLE_TERRAIN_HPP
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "IncludeVulkan.hpp"
#include "HostRenderData/HostRenderData.hpp"
#include "ImageLoader/Image.hpp"

namespace shuttle
{

struct TerrainProperties
{
    vk::Extent2D meshResolution;
    glm::vec2 worldSize;

    float minHeight = -10.0f;
    float maxHeight = 50.0f;

    glm::vec2 textureRepeatFactor{100.0f, 100.0f};
};

// 2. Генератор
class TerrainGeometryGenerator
{
  public:
    static HostMeshData createFromHeightMap(const TerrainProperties& props, const Image1D16bit& heightMap);
};
} // namespace shuttle

#endif // HELLOTRIANGLE_TERRAIN_HPP
