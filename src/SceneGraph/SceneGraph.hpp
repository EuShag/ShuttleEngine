//
// Created by Shagu on 07.04.2026.
//

#ifndef HELLOTRIANGLE_SCENEGRAPH_HPP
#define HELLOTRIANGLE_SCENEGRAPH_HPP

#include "IncludeVulkan.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera/Camera.hpp"

namespace render {


    class SceneGraph {
    public:
        SceneGraph(aiScene* scene);
    private:

    };
} // render

#endif //HELLOTRIANGLE_SCENEGRAPH_HPP