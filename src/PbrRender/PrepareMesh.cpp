//
// Created by Shagu on 30.05.2026.
//
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"
#include <stack>
#include <glm/gtx/string_cast.hpp>

namespace shuttle_engine {

    struct DrawInstance {
        glm::mat4 transform;
        uint32_t materialIndex;
        uint32_t meshIndex;
    };

    struct MeshDescription {
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
    };

    MeshData PbrRender::prepareHostMeshData(
        HostSceneData const &hostSceneData
    ) {

        if (hostSceneData.meshes.empty() || hostSceneData.materials.empty()) {
            return {};
        }

        std::vector<DrawInstance> drawInstances;

        // 1. Храним в стеке УКАЗАТЕЛИ на ноды, чтобы избежать глубокого копирования
        std::stack<std::pair<glm::mat4, const HostNode*>> sceneNodesStack;

        // Инициализируем стек указателем на корень
        sceneNodesStack.emplace(glm::mat4(1.0f), &hostSceneData.rootNode);

        while (!sceneNodesStack.empty()) {
            // Получаем parentTransform и указатель на текущую ноду
            auto [parentTransform, currentNodePtr] = sceneNodesStack.top();
            sceneNodesStack.pop();

            // Разыменовываем указатель для удобства (ссылка бесплатна)
            const auto& currentNode = *currentNodePtr;

            // 2. Вычисляем мировую матрицу текущей ноды ОДИН раз
            glm::mat4 currentWorldTransform = currentNode.localTransform * parentTransform;

            if (!currentNode.meshes.empty()) {
                // Резервируем место в векторе, если мешей много (хороший тон для перформанса)
                drawInstances.reserve(drawInstances.size() + currentNode.meshes.size());

                for (const auto& meshInstance : currentNode.meshes) {
                    drawInstances.push_back(DrawInstance{
                        .transform = currentWorldTransform, // Используем уже готовую матрицу
                        .materialIndex = meshInstance.materialIndex,
                        .meshIndex = meshInstance.meshIndex
                    });
                }
            }

            // 3. Добавляем детей в стек (передаем только их адреса)
            for (const auto& child : currentNode.children) {
                sceneNodesStack.emplace(currentWorldTransform, &child);
            }
        }

        // 2. Сортировка мешей по материалам и мешам
        std::ranges::sort(drawInstances, [](const DrawInstance& a, const DrawInstance& b) {
            if (a.materialIndex != b.materialIndex) {
                return a.materialIndex < b.materialIndex;
            }
            return a.meshIndex < b.meshIndex; // <-- ЭТО КЛЮЧЕВОЕ ДОБАВЛЕНИЕ
        });

        std::vector<MeshDescription> meshes{};
        meshes.reserve(hostSceneData.meshes.size());

        std::vector<PositionAttribute> positionsData;
        std::vector<NormalTangentUvAttribute> normalTangentUvData;
        std::vector<uint32_t> indices{};

        uint32_t vertexCount{0};
        uint32_t indexCount{0};
        for (auto&& mesh : hostSceneData.meshes) {
            vertexCount += mesh.positions.size();
            indexCount += mesh.indices.size();
        }

        positionsData.resize(vertexCount);
        normalTangentUvData.resize(vertexCount);
        indices.resize(indexCount);

        uint32_t currentIndexOffset{0};
        int32_t currentVertexOffset{0};

        for (const auto& mesh : hostSceneData.meshes) {

            // Копируем атрибуты (позиции, нормали, тангенты, UV)
            for (size_t i = 0; i < mesh.positions.size(); ++i) {
                positionsData[currentVertexOffset + i] = {
                    .position = mesh.positions[i]
                };

                normalTangentUvData[currentVertexOffset + i] = {
                    .normal = mesh.normals[i],
                    .uv = mesh.uvs[i],
                    .tangent = mesh.tangents[i]
                };
            }

            // 2. Копируем индексы (с учетом смещения!)
            for (size_t i = 0; i < mesh.indices.size(); ++i) {
                indices[currentIndexOffset + i] = mesh.indices[i];
            }

            // 3. Записываем метаданные меша
            meshes.push_back({
                .indexCount = static_cast<uint32_t>(mesh.indices.size()),
                .firstIndex = currentIndexOffset,
                .vertexOffset = currentVertexOffset
            });

            // 4. Обновляем смещения для следующего меша
            currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());
            currentVertexOffset += static_cast<int32_t>(mesh.positions.size());
        }

        std::vector<vk::DrawIndexedIndirectCommand> indirectCommands{};

        std::vector<IndirectDraw> indirectDraws{};
        indirectDraws.reserve(hostSceneData.materials.size());

        std::vector<ModelData> modelMatrices{};
        modelMatrices.reserve(drawInstances.size());

        uint32_t currentIndirectDrawCommandCount{0};
        uint32_t currentFirstIndirectDrawCommandIndex{0};
        uint32_t currentInstanceIndex{0};
        uint32_t currentInstanceCount{0};
        uint32_t currentMaterialIndex = drawInstances.begin()->materialIndex;
        uint32_t currentMeshIndex = drawInstances.begin()->meshIndex;

        for (auto&& drawInstance : drawInstances) {
            bool meshChanged = drawInstance.meshIndex != currentMeshIndex;
            bool materialChanged = drawInstance.materialIndex != currentMaterialIndex;

            if (meshChanged || materialChanged) {

                // Фиксируем нашу команду
                const auto& meshDesc = meshes[currentMeshIndex];

                // Пушим нашу команду
                indirectCommands.push_back({
                    .indexCount = meshDesc.indexCount,
                    .instanceCount = currentInstanceCount,
                    .firstIndex = meshDesc.firstIndex,
                    .vertexOffset = meshDesc.vertexOffset,
                    .firstInstance = currentInstanceIndex
                });
                currentIndirectDrawCommandCount++; // Мы добавили команду
                currentInstanceIndex += currentInstanceCount; // Теперь мы смещаем индекс смещён на количество отрисованных экземпляров
                currentInstanceCount = 0; // Сбрасываем до нуля количество экземпляров
                currentMeshIndex = drawInstance.meshIndex; // Обновляем текущий меш

                if (materialChanged) {
                    indirectDraws.push_back(IndirectDraw{
                        .materialIndex = currentMaterialIndex,
                        .commandCount = currentIndirectDrawCommandCount,
                        .indirectBufferOffset = currentFirstIndirectDrawCommandIndex * static_cast<uint32_t>(sizeof(vk::DrawIndexedIndirectCommand))
                    });

                    currentMaterialIndex = drawInstance.materialIndex;

                    currentFirstIndirectDrawCommandIndex += currentIndirectDrawCommandCount; // Смещаем индекс на количество отрисованных команд
                    currentIndirectDrawCommandCount = 0; // Сбрасываем до нуля количество команд
                }
            }

            modelMatrices.push_back({
                .modelMatrix = drawInstance.transform,
                .normalMatrix = glm::mat3(glm::transpose(glm::inverse(drawInstance.transform)))
            });
            currentInstanceCount++;
        }

        // Записываем "Хвосты" (последний меш и последний материал)
        const auto& lastMeshDesc = meshes[currentMeshIndex];
        indirectCommands.push_back({
            .indexCount = lastMeshDesc.indexCount,
            .instanceCount = currentInstanceCount,
            .firstIndex = lastMeshDesc.firstIndex,
            .vertexOffset = lastMeshDesc.vertexOffset,
            .firstInstance = currentInstanceIndex
        });

        indirectDraws.push_back({
            .materialIndex = currentMaterialIndex,
            .commandCount = static_cast<uint32_t>(indirectCommands.size()) - currentFirstIndirectDrawCommandIndex,
            .indirectBufferOffset = currentFirstIndirectDrawCommandIndex * static_cast<uint32_t>(sizeof(vk::DrawIndexedIndirectCommand))
        });

        return {
            .positionData = std::move(positionsData),
            .normalUvTangentData = std::move(normalTangentUvData),
            .indices = std::move(indices),
            .indirectCommands = std::move(indirectCommands),
            .modelDatas = std::move(modelMatrices),
            .indirectDrawCalls = std::move(indirectDraws),
        };
    }



    vk::ResultValue<StagingBufferMeshData> PbrRender::prepareStagingBufferMeshData(
        MeshData const &meshData,
        resources::DeviceAllocator const& deviceAllocator,
        resources::AllocatedBuffer stagingBuffer,
        vk::DeviceSize& stagingBufferOffset
    ) {
        const vk::DeviceSize positionsSize = meshData.positionData.size() * sizeof(PositionAttribute);
        const vk::DeviceSize attribsSize   = meshData.normalUvTangentData.size() * sizeof(NormalTangentUvAttribute);
        const vk::DeviceSize indicesSize   = meshData.indices.size() * sizeof(uint32_t);
        const vk::DeviceSize commandsSize  = meshData.indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand);
        const vk::DeviceSize modelsSize    = meshData.modelDatas.size() * sizeof(ModelData);

        // 1. Вычисляем локальные смещения относительно текущего stagingBufferOffset
        vk::DeviceSize positionAttributeOffset = stagingBufferOffset;
        vk::DeviceSize normalUvTangentAttributeOffset = positionAttributeOffset + positionsSize;
        vk::DeviceSize indicesOffset  = normalUvTangentAttributeOffset + attribsSize;
        vk::DeviceSize indirectCommandsOffset  = indicesOffset + indicesSize;
        vk::DeviceSize modelDatasOffset = indirectCommandsOffset + commandsSize;

        // 2. Обновляем внешний offset для СЛЕДУЮЩЕГО вызова функции
        // ВАЖНО: Добавляем выравнивание, чтобы следующий меш начался с "чистого" адреса
        // 256 байт — безопасный вариант для большинства архитектур (Alignment для SSBO)
        stagingBufferOffset = alignUp(modelDatasOffset + modelsSize, vk::DeviceSize(256));


        if (auto result = deviceAllocator.writeBufferFromHost({
            .dstBuffer = stagingBuffer,
            .dstBufferOffset = positionAttributeOffset,
            .srcData = meshData.positionData.data(),
            .dataSize = meshData.positionData.size() * sizeof(PositionAttribute),
        }); result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = deviceAllocator.writeBufferFromHost({
            .dstBuffer = stagingBuffer,
            .dstBufferOffset = normalUvTangentAttributeOffset,
            .srcData = meshData.normalUvTangentData.data(),
            .dataSize = meshData.normalUvTangentData.size() * sizeof(NormalTangentUvAttribute),
        }); result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = deviceAllocator.writeBufferFromHost({
            .dstBuffer = stagingBuffer,
            .dstBufferOffset = indicesOffset,
            .srcData = meshData.indices.data(),
            .dataSize = meshData.indices.size() * sizeof(uint32_t),
        }); result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = deviceAllocator.writeBufferFromHost({
            .dstBuffer = stagingBuffer,
            .dstBufferOffset = indirectCommandsOffset,
            .srcData = meshData.indirectCommands.data(),
            .dataSize = meshData.indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand),
        }); result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = deviceAllocator.writeBufferFromHost({
            .dstBuffer = stagingBuffer,
            .dstBufferOffset = modelDatasOffset,
            .srcData = meshData.modelDatas.data(),
            .dataSize = meshData.modelDatas.size() * sizeof(ModelData),
        }); result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // Заполняем структуру с информацией о размещении данных в staging buffer
        return {
            vk::Result::eSuccess,
            {
                .stagingBuffer = stagingBuffer,
                .vertexBufferSize = positionsSize + attribsSize,
                .positionAttributeVertexBindingBufferOffset = positionAttributeOffset,
                .normalUvTangentAttributeVertexBindingBufferOffset = normalUvTangentAttributeOffset,
                .indexBufferSize = indicesSize,
                .indicesBufferOffset = indicesOffset,
                .indirectCommandsBufferSize = commandsSize,
                .indirectCommandsBufferOffset = indirectCommandsOffset,
                .modelDatasBufferSize = modelsSize,
                .modelDatasBufferOffset = modelDatasOffset,
                .indirectDrawCalls = meshData.indirectDrawCalls
            }
        };
    }

    void StagingBufferMeshData::recordCopyCommandsToBuffer(
        vk::CommandBuffer cmdBuffer,
        vk::Buffer vertexBuffer,
        vk::Buffer indexBuffer,
        vk::Buffer indirectCommandsBuffer,
        vk::Buffer modelDatasBuffer) const {
        cmdBuffer.copyBuffer(
            stagingBuffer,
            vertexBuffer,
            {
                vk::BufferCopy{
                    .srcOffset = positionAttributeVertexBindingBufferOffset,
                    .dstOffset = 0,
                    .size = vertexBufferSize
                }
            }
        );

        cmdBuffer.copyBuffer(
            stagingBuffer,
            indexBuffer,
            {
                vk::BufferCopy{
                    .srcOffset = indicesBufferOffset,
                    .dstOffset = 0,
                    .size = indexBufferSize
                }
            }
        );

        cmdBuffer.copyBuffer(
            stagingBuffer,
            indirectCommandsBuffer,
            {
                vk::BufferCopy{
                    .srcOffset = indirectCommandsBufferOffset,
                    .dstOffset = 0,
                    .size = indirectCommandsBufferSize
                }
            }
        );

        cmdBuffer.copyBuffer(
            stagingBuffer,
            modelDatasBuffer,
            {
                vk::BufferCopy{
                    .srcOffset = modelDatasBufferOffset,
                    .dstOffset = 0,
                    .size = modelDatasBufferSize
                }
            }
        );
    }

    vk::ResultValue<DeviceMeshData> PbrRender::prepareDeviceMeshData(
        const StagingBufferMeshData& stagingInfo,
        resources::DeviceAllocator const& deviceAllocator)
    {
        DeviceMeshData deviceMesh;

        // 1. Создаем Вершинный буфер (Vertex Buffer)
        // Мы создаем один буфер, но в нем будут лежать два потока данных:
        // сначала все позиции, потом все атрибуты (нормали, UV, тангенты).
        vk::BufferCreateInfo vertexBufferInfo{
            .size = stagingInfo.vertexBufferSize,
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive
        };

        auto vbRes = deviceAllocator.createAndAllocateBufferUnique(vertexBufferInfo, resources::MemoryUsage::eGpuOnly);
        if (vbRes.result != vk::Result::eSuccess) return {vbRes.result, {}};
        deviceMesh.vertexBuffer = std::move(vbRes.value);

        // Смещения внутри буфера мы берем из staging-плана
        // (Позиции начинаются с 0, атрибуты со смещением)
        deviceMesh.positionAttributeOffset = 0;
        deviceMesh.normalUvTangentAttributeOffset = stagingInfo.normalUvTangentAttributeVertexBindingBufferOffset - stagingInfo.positionAttributeVertexBindingBufferOffset;

        // 2. Создаем Индексный буфер (Index Buffer)
        vk::BufferCreateInfo indexBufferInfo{
            .size = stagingInfo.indexBufferSize,
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive
        };

        auto ibRes = deviceAllocator.createAndAllocateBufferUnique(indexBufferInfo, resources::MemoryUsage::eGpuOnly);
        if (ibRes.result != vk::Result::eSuccess) return {ibRes.result, {}};
        deviceMesh.indexBuffer = std::move(ibRes.value);
        deviceMesh.indexBufferOffset = 0;

        // 3. Создаем Indirect-буфер (Буфер команд для Multi-Draw Indirect)
        vk::BufferCreateInfo indirectBufferInfo{
            .size = stagingInfo.indirectCommandsBufferSize,
            .usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive
        };

        auto indRes = deviceAllocator.createAndAllocateBufferUnique(indirectBufferInfo, resources::MemoryUsage::eGpuOnly);
        if (indRes.result != vk::Result::eSuccess) return {indRes.result, {}};
        deviceMesh.indirectBuffer = std::move(indRes.value);
        deviceMesh.indirectBufferOffset = 0;

        // 4. Создаем SSBO-буфер для матриц трансформации моделей
        vk::BufferCreateInfo ssboInfo{
            .size = stagingInfo.modelDatasBufferSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive
        };

        auto ssboRes = deviceAllocator.createAndAllocateBufferUnique(ssboInfo, resources::MemoryUsage::eGpuOnly);
        if (ssboRes.result != vk::Result::eSuccess) return {ssboRes.result, {}};
        deviceMesh.modelSsboBuffer = std::move(ssboRes.value);
        deviceMesh.modelSsboBufferOffset = 0;

        // Копируем описание вызовов (Draw Calls)
        deviceMesh.indirectDraws = stagingInfo.indirectDrawCalls;

        return {vk::Result::eSuccess, std::move(deviceMesh)};
    }

}