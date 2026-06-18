//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_RAWRENDERDATA_CPP_HPP
#define HELLOTRIANGLE_RAWRENDERDATA_CPP_HPP
#include "HostRenderData.hpp"

namespace shuttle_engine {
    void HostSceneData::merge(const HostSceneData& other, const glm::mat4& transform) {
        // 1. Запоминаем текущие размеры наших пулов.
        // Это и есть смещения (офсеты) для индексов добавляемой сцены.
        auto meshOffset = static_cast<uint32_t>(meshes.size());
        auto materialOffset = static_cast<uint32_t>(materials.size());

        // 2. Резервируем память, чтобы избежать частых реаллокаций векторов
        meshes.reserve(meshes.size() + other.meshes.size());
        materials.reserve(materials.size() + other.materials.size());

        // 3. Копируем меши и материалы из другой сцены в наши глобальные пулы
        meshes.insert(meshes.end(), other.meshes.begin(), other.meshes.end());
        materials.insert(materials.end(), other.materials.begin(), other.materials.end());

        // 4. Копируем корневую ноду добавляемой сцены
        HostNode importedRoot = other.rootNode;
        // 5. Корректируем индексы мешей и материалов в скопированном дереве нод
        offsetNodeIndices(importedRoot, meshOffset, materialOffset);

        HostNode importedRootNode{};
        importedRootNode.name = "ImportedRoot: " + other.rootNode.name;
        importedRootNode.children.push_back(std::move(importedRoot));
        importedRootNode.localTransform = transform;

        rootNode.children.push_back(std::move(importedRootNode));


        // Примечание: Параметры солнца (sun) мы оставляем от хост-сцены,
        // так как в одном мире может быть только одно солнце.
    }

    void HostSceneData::addTerrain(
    const HostMeshData &mesh,
    const HostMaterialData &material,
    const glm::mat4 &transform)
    {
        // 1. Сохраняем данные в глобальные массивы
        meshes.emplace_back(mesh);
        materials.emplace_back(material);

        // Получаем индексы (они будут последними в списке)
        auto meshIndex = static_cast<uint32_t>(meshes.size() - 1);
        auto matIndex = static_cast<uint32_t>(materials.size() - 1);

        // 2. Создаем узел для террейна
        HostNode terrainNode{};
        terrainNode.localTransform = transform;
        terrainNode.meshes.emplace_back(meshIndex, matIndex);
        terrainNode.name = "TerrainNode";

        // 3. Безопасное обновление дерева
        // Предполагаем, что у тебя есть поле HostNode rootNode в классе HostSceneData

        // Создаем новый корень, который будет родительским для старого корня и нового террейна
        HostNode newRootNode{};
        newRootNode.name = "SceneRoot";

        // Перемещаем старый корень в дети нового
        newRootNode.children.push_back(std::move(rootNode));

        // Добавляем террейн как нового ребенка
        newRootNode.children.push_back(std::move(terrainNode));

        // Обновляем текущий корень
        rootNode = std::move(newRootNode);
        rootNode.localTransform = glm::mat4(1);
    }
    
    void HostSceneData::offsetNodeIndices(HostNode& node, uint32_t meshOffset, uint32_t materialOffset) {
        // Смещаем индексы для всех мешей, привязанных к текущей ноде
        for (auto&[meshIndex, materialIndex] : node.meshes) {
            meshIndex += meshOffset;
            materialIndex += materialOffset;
        }

        // Рекурсивно идем вглубь по дереву детей
        for (auto& child : node.children) {
            offsetNodeIndices(child, meshOffset, materialOffset);
        }
    }
}

#endif //HELLOTRIANGLE_RAWRENDERDATA_CPP_HPP
