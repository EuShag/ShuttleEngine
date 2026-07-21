//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_RAWRENDERDATA_HPP
#define HELLOTRIANGLE_RAWRENDERDATA_HPP
#include <vector>
#include <glm/glm.hpp>

namespace shuttle_engine {

    // Used by TerrainGeometryGenerator and terrain-related upload paths.
    struct HostMeshData {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec4> tangents;
        std::vector<uint32_t> indices;
    };

    // PBR material properties uploaded as a UBO (binding 0 in the material descriptor set).
    struct HostMaterialProperties {
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};

        float metallicFactor{0.0f};
        float roughnessFactor{1.0f};
        float occlusionStrength{1.0f};
        float emissiveStrength{0.0f};

        glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
        float padding{0.0f};
    };


}

#endif //HELLOTRIANGLE_RAWRENDERDATA_HPP
