//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_ASSIMPLOADER_HPP
#define HELLOTRIANGLE_ASSIMPLOADER_HPP
#include <assimp/Importer.hpp>

#include "../HostRenderData/HostRenderData.hpp"


namespace shuttle_engine {
    class AssimpLoader {
    public:
        HostSceneData loadScene(const std::string &filename);
    private:
        Assimp::Importer importer;
    };
} // shuttle_engine

#endif //HELLOTRIANGLE_ASSIMPLOADER_HPP
