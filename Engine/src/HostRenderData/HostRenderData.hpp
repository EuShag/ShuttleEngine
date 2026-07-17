//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_RAWRENDERDATA_HPP
#define HELLOTRIANGLE_RAWRENDERDATA_HPP
#include <glm/glm.hpp>

#include "IncludeVulkan.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"

namespace shuttle_engine {

    struct HostMeshData {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec4> tangents;
        std::vector<uint32_t> indices;
    };

    struct HostMaterialProperties {
        // 16 байт
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f}; // Белый цвет, полная непрозрачность

        // 4 + 4 + 4 + 4 = 16 байт
        float metallicFactor{0.0f};           // По умолчанию диэлектрик (не металл)
        float roughnessFactor{1.0f};          // По умолчанию матовый (безопасное PBR-значение)
        float occlusionStrength{1.0f};        // Максимальное влияние AO
        float emissiveStrength{0.0f};         // По умолчанию свечение выключено

        // 12 + 4 = 16 байт
        glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f}; // Черный цвет (нет свечения)
        float padding{0.0f};                        // Явное обнуление паддинга
    };


    struct MipInfo {
        uint32_t width;
        uint32_t height;
        size_t offset;
        size_t size;
    };

    struct HostImageData {
        uint32_t width{}, height{};
        vk::Format imageFormat = vk::Format::eR8G8B8A8Unorm;
        std::vector<uint8_t> data; // Все уровни подряд
        std::vector<MipInfo> mipChain;

        // Конструктор по умолчанию (уже есть)
        HostImageData() = default;

        // Конструктор перемещения
        HostImageData(HostImageData&& other) noexcept
            : width(other.width),
              height(other.height),
              imageFormat(other.imageFormat),
              data(std::move(other.data)), // Ключевой момент!
              mipChain(std::move(other.mipChain)) {}

        // Оператор присваивания перемещением
        HostImageData& operator=(HostImageData&& other) noexcept {
            if (this != &other) {
                width = other.width;
                height = other.height;
                imageFormat = other.imageFormat;
                data = std::move(other.data); // Ключевой момент!
                mipChain = std::move(other.mipChain);
                // Очищаем источник (не обязательно, но хорошая практика)
                other.width = 0; other.height = 0; other.data.clear();
            }
            return *this;
        }

        HostImageData(HostImageData const& other) = default;
        HostImageData& operator=(HostImageData const& other) = default;

        // Дополнительные методы
        [[nodiscard]] bool isEmpty() const { return data.empty(); }
    };



    struct HostMaterialData {

        HostMaterialProperties materialProperties;

        std::optional<HostImageData> albedoTexture;
        std::optional<HostImageData> normalTexture;
        std::optional<HostImageData> ormTexture;
        std::optional<HostImageData> emissiveTexture;
        std::optional<HostImageData> heightTexture;
    };

    struct HostMeshInstance {
        uint32_t meshIndex{0};
        uint32_t materialIndex{0};
    };

    struct HostNode {
        std::string name;
        glm::mat4 localTransform;
        std::vector<HostMeshInstance> meshes;
        std::vector<HostNode> children;
    };

    struct HostDirectionalLight {
        glm::vec4 direction{0.5f, 1.0f, 0.3f, 1.0f}; // Направление (куда светит солнце, вниз и немного вбок)
        glm::vec4 color{1.0f, 0.95f, 0.9f, 1.0f};       // Цвет света (чуть желтоватый, как настоящее солнце)
    };

    struct HostSceneData {
        std::vector<HostMeshData> meshes;
        std::vector<HostMaterialData> materials;

        HostNode rootNode;

        HostDirectionalLight sunLight;

        glm::vec4 ambientLight{0.2f, 0.5f, 1.0f, 0.1f};

        void merge(const HostSceneData& other, const glm::mat4& transform = glm::mat4(1.0f));
        void addTerrain(const HostMeshData& mesh, const HostMaterialData& material, const glm::mat4& transform = glm::mat4(1.0f));

    private:
        static void offsetNodeIndices(HostNode& node, uint32_t meshOffset, uint32_t materialOffset);
    };

}

#endif //HELLOTRIANGLE_RAWRENDERDATA_HPP
