//
// Created by Shagu on 24.05.2026.
//

#ifndef HELLOTRIANGLE_SCENE_HPP
#define HELLOTRIANGLE_SCENE_HPP

#include <vector>
#include "IncludeVulkan.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include <assimp/scene.h>
#include <glm/glm.hpp>

#include "Camera/Camera.hpp"

namespace render {

    // 1. Направленный свет (Солнце) - нет позиции, только направление
    struct alignas(16) DirectLight {
        glm::vec4 colorAndIntensity; // rgb - цвет, w - яркость
        glm::vec4 direction;        // xyz - направление, w - не используется (padding)
    };

    // 2. Точечный свет (Лампочка) - светит во все стороны из точки
    struct alignas(16) PointLight {
        glm::vec4 colorAndIntensity; // rgb - цвет, w - яркость
        glm::vec4 position;         // xyz - позиция, w - радиус действия (range)
    };

    // 3. Прожектор (Фонарик) - светит конусом из точки в направлении
    struct alignas(16) SpotLight {
        glm::vec4 colorAndIntensity; // rgb - цвет, w - яркость
        glm::vec4 position;         // xyz - позиция, w - радиус действия
        glm::vec4 direction;        // xyz - направление, w - косинус внутреннего угла конуса
        glm::vec4 params;           // x - косинус внешнего угла конуса, yzw - padding
    };

    struct alignas(16) PbrMaterialParams {
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f}; // rgb - цвет, a - прозрачность
        glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};       // rgb - цвет свечения
        float emissiveStrength{1.0f};                     // Интенсивность свечения (HDR)

        float metallicFactor{0.0f};                       // 0.0 - диэлектрик, 1.0 - металл
        float roughnessFactor{1.0f};                      // 0.0 - гладкий, 1.0 - шероховатый
        float occlusionStrength{1.0f};                    // Сила влияния карты AO (от 0.0 до 1.0)
    }; // Общий размер: 16 + 16 + 16 = 48 байт. Идеально.

    struct PbrMaterialData {
        PbrMaterialParams pbrMaterialParams;

        vk::ImageView albedoTexture;
        vk::ImageView normalTexture;
        vk::ImageView ormTexture; // Occlusion, Roughness, Metallic
        vk::ImageView emissiveTexture;

        vk::Sampler textureSampler;
    };

    struct PbrLightData {

    };

    class Node;

    struct Mesh {
        std::vector<std::pair<vk::Buffer, uint32_t>> vertexBuffers;
        std::vector<std::pair<vk::Buffer, uint32_t>> indexBuffers;
    };

    class PbrScene {


    public:
        // light sources
        std::vector<DirectLight> directionalLights;
        std::vector<PointLight> pointLights;
        std::vector<SpotLight> spotLights;
        // meshes
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        // cameras
        std::vector<vm::Camera> cameras;
    };

    class PbrRender {



        Scene createScene();
        Scene addNode(Scene const & scene, Node const & parent, Node const & node);

        AssimpLoader* createAssimpLoader();

        void renderScene(Scene& scene);

    private:
        std::vector<vk::DescriptorSetLayout> objectDescriptorSetLayouts;
        vk::PipelineLayout objectPipelineLayout;
        vk::Pipeline objectPipeline;

    };


}

#endif //HELLOTRIANGLE_SCENE_HPP