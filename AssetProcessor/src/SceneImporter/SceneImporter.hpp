//
// Created by Shagu on 06.07.2026.
//

#ifndef SHUTTLEENGINE_SCENEIMPORTER_HPP
#define SHUTTLEENGINE_SCENEIMPORTER_HPP
#include <vector>
#include <assimp/scene.h>
#include <vulkan/vulkan.hpp>

#include "BlobLayout.hpp"

namespace shuttle_engine::assets{

    struct MeshMetaInfo {
        uint64_t positionDataOffset;
        uint64_t uvNormalTangentDataOffset;
        uint64_t jointIdsWeightsDataOffset;
        uint64_t indexDataOffset[8];
        uint32_t indexCount[8];
        uint32_t vertexCount;
        uint32_t materialIndex;
        uint32_t lodCount;
    };

    class SceneImporter {
    public:
        static bool loadScene();

    private:
        static bool processNode(aiNode* node);
        static bool processMesh(aiMesh* mesh);
        static bool processMaterial(aiMaterial* material);
        static bool processSkeleton(aiSkeleton* skeleton);

    };
}
#endif //SHUTTLEENGINE_SCENEIMPORTER_HPP
