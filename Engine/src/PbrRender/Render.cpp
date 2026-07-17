//
// Created by Shagu on 25.05.2026.
//
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANIBO
#include "Render.hpp"

#include <iostream>
#include <ostream>
#include <stack>
#include <utility>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "assimp/Vertex.h"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine {
    void PreparedHostMaterialData::calculateOffsetsAndSize() {
        // Вспомогательная лямбда для выравнивания (по 4 байта для текстур, 16 для UBO)
        auto align = [](vk::DeviceSize size, vk::DeviceSize alignment) {
            return (size + (alignment - 1)) & ~(alignment - 1);
        };

        vk::DeviceSize current = 0;

        // 1. Uniform Buffer (выравнивание 16 байт)
        stagingBufferOffsets.propertiesOffset = current;
        current += align(sizeof(HostMaterialProperties), 16);

        // 2. Текстуры (выравнивание 4 байта)
        stagingBufferOffsets.albedoOffset = current;
        current += align(albedoHostImageData.data.size(), 4);

        stagingBufferOffsets.normalOffset = current;
        current += align(normalHostImageData.data.size(), 4);

        stagingBufferOffsets.ormOffset = current;
        current += align(ormHostImageData.data.size(), 4);

        stagingBufferOffsets.emissionOffset = current;
        current += align(emissionHostImageData.data.size(), 4);

        stagingBufferOffsets.heightOffset = current;
        current += align(heightHostImageData.data.size(), 4);

        // Итоговый размер
        stagingBufferRequiredSize = current;
    }

    vk::ResultValue<PbrRender> PbrRender::create(vk::Device device, vk::ImageLayout finalLayout) {
        PbrRender render;

        // 1. Инициализация RenderPasses
        if (auto res = render.initMainRenderPass(device, finalLayout); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initShadowRenderPass(device); res != vk::Result::eSuccess)
            return {res, {}};

        // 2. Инициализация Descriptor Set Layouts
        if (auto res = render.initPbrMaterialSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initSceneDataSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initSamplerDescriptorSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initModelDataSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initSamplers(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initSamplerDescriptorSet(device); res != vk::Result::eSuccess)
            return {res, {}};

        // 3. Инициализация Pipeline Layouts (требуют готовых DescriptorSetLayouts)
        if (auto res = render.initMainPipelineLayout(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initShadowPipelineLayout(device); res != vk::Result::eSuccess)
            return {res, {}};

        // 4. Инициализация Pipelines (требуют готовых RenderPasses и PipelineLayouts)
        if (auto res = render.initMainPipeline(device); res != vk::Result::eSuccess)
            return {res, {}};

        if (auto res = render.initShadowPipeline(device); res != vk::Result::eSuccess)
            return {res, {}};

        // Возвращаем успешно созданный объект
        return {vk::Result::eSuccess, std::move(render)};
    }

    vk::ResultValue<DeviceSceneData> PbrRender::uploadScene(
        HostSceneData&& hostSceneData,
        vk::Queue transferQueue,
        vk::Device device,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator)
    {
        DeviceSceneData resultData;

        // 1. ПОДГОТОВКА И СОРТИРОВКА ДАННЫХ ГЕОМЕТРИИ И МАТЕРИАЛОВ (CPU)
        MeshData hostMeshData = prepareHostMeshData(hostSceneData);

        vk::DeviceSize geometryRequiredSize = 0;
        geometryRequiredSize += hostMeshData.positionData.size() * sizeof(PositionAttribute);
        geometryRequiredSize += hostMeshData.normalUvTangentData.size() * sizeof(NormalTangentUvAttribute);
        geometryRequiredSize += hostMeshData.indices.size() * sizeof(uint32_t);
        geometryRequiredSize += hostMeshData.indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand);
        geometryRequiredSize += hostMeshData.modelDatas.size() * sizeof(ModelData);
        geometryRequiredSize = alignUp(geometryRequiredSize, static_cast<vk::DeviceSize>(256));

        auto const matCount = static_cast<uint32_t>(hostSceneData.materials.size());
        std::vector<PreparedHostMaterialData> preparedMaterials;
        preparedMaterials.reserve(matCount);

        vk::DeviceSize totalMaterialsStagingSize = 0;
        for (uint32_t i = 0; i < matCount; ++i) {
            auto prepared = prepareHostMaterialData(hostSceneData.materials[i]);
            totalMaterialsStagingSize += alignUp(prepared.stagingBufferRequiredSize, static_cast<vk::DeviceSize>(256));
            preparedMaterials.push_back(std::move(prepared));
        }

        // 2. ВЫДЕЛЕНИЕ ГЛОБАЛЬНОГО STAGING BUFFER И ЗАПИСЬ ДАННЫХ
        vk::DeviceSize const totalStagingSize = geometryRequiredSize + totalMaterialsStagingSize;

        auto [stgRes, stagingBuffer] = allocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = totalStagingSize,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive
            },
            resources::MemoryUsage::eCpuOnly
        );
        if (stgRes != vk::Result::eSuccess) return {stgRes, {}};

        vk::DeviceSize currentStagingOffset = 0;

        auto [stgMeshRes, stagingMeshInfo] = prepareStagingBufferMeshData(
            hostMeshData, allocator, *stagingBuffer, currentStagingOffset
        );
        if (stgMeshRes != vk::Result::eSuccess) return {stgMeshRes, {}};

        auto [devMeshRes, deviceMeshData] = prepareDeviceMeshData(stagingMeshInfo, allocator);
        if (devMeshRes != vk::Result::eSuccess) return {devMeshRes, {}};

        resultData.vertexBuffer = std::move(deviceMeshData.vertexBuffer);
        resultData.positionAttributeOffset = deviceMeshData.positionAttributeOffset;
        resultData.normalUvTangentAttributeOffset = deviceMeshData.normalUvTangentAttributeOffset;

        resultData.indexBuffer = std::move(deviceMeshData.indexBuffer);
        resultData.indexBufferOffset = deviceMeshData.indexBufferOffset;

        resultData.indirectDrawBuffer = std::move(deviceMeshData.indirectBuffer);
        resultData.indirectDrawBufferOffset = deviceMeshData.indirectBufferOffset;
        resultData.indirectDraws = std::move(deviceMeshData.indirectDraws);

        resultData.modelSsbo = std::move(deviceMeshData.modelSsboBuffer);

        // 3. СОЗДАНИЕ DESCRIPTOR POOL
        uint32_t const totalSetsCount = matCount + 1;

        std::array<vk::DescriptorPoolSize, 3> poolSizes{{
            { vk::DescriptorType::eUniformBuffer, matCount },
            { vk::DescriptorType::eStorageBuffer, 1 },
            { vk::DescriptorType::eSampledImage,  matCount * 5 }
        }};

        auto [poolRes, uniqueDescriptorPool] = device.createDescriptorPoolUnique({
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = totalSetsCount,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        });
        if (poolRes != vk::Result::eSuccess) return {poolRes, {}};

        // 4. ПОДГОТОВКА И ЗАПОЛНЕНИЕ ДЕСКРИПТОРОВ МАТЕРИАЛОВ (Set 1)
        std::vector materialLayouts(matCount, *pbrMaterialSetLayout);
        auto materialSetsRes = device.allocateDescriptorSets({
            .descriptorPool = *uniqueDescriptorPool,
            .descriptorSetCount = matCount,
            .pSetLayouts = materialLayouts.data()
        });
        if (materialSetsRes.result != vk::Result::eSuccess) return {materialSetsRes.result, {}};

        resultData.materials.reserve(matCount);
        std::vector<StagingBufferMaterialInfo> stagingMatInfos;
        stagingMatInfos.reserve(matCount);

        for (uint32_t i = 0; i < matCount; ++i) {
            auto [stgMatRes, stagingMatInfo] = prepareStagingBufferMaterialInfo(
                std::move(preparedMaterials[i]), allocator, *stagingBuffer, currentStagingOffset
            );
            if (stgMatRes != vk::Result::eSuccess) return {stgMatRes, {}};

            auto [devMatRes, deviceMatInfo] = prepareDeviceMaterialInfo(stagingMatInfo, device, allocator);
            if (devMatRes != vk::Result::eSuccess) return {devMatRes, {}};

            fillDescriptorSet(device, materialSetsRes.value[i], deviceMatInfo);

            DeviceSceneData::RenderMaterialData renderMat{
                .deviceMaterialInfo = std::move(deviceMatInfo),
                .materialSet = std::move(materialSetsRes.value[i]),
            };

            resultData.materials.push_back(std::move(renderMat));
            stagingMatInfos.push_back(std::move(stagingMatInfo));
        }

        auto [createModelSsboSetResult, modelSsboSet] = device.allocateDescriptorSets(
            {
                .descriptorPool = *uniqueDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &*modelSetLayout
            }
        );

        if (createModelSsboSetResult != vk::Result::eSuccess) return {createModelSsboSetResult, {}};

        resultData.modelSsboDescriptorSet = std::move(modelSsboSet[0]);
        resultData.descriptorPool = std::move(uniqueDescriptorPool);

        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *resultData.modelSsbo,
            .offset = 0,
            .range = vk::WholeSize
        };

        device.updateDescriptorSets(
            {
                vk::WriteDescriptorSet{
                    .dstSet = resultData.modelSsboDescriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &bufferInfo
                }
            },
            {}
        );

        // 6. СБОРКА И ОТПРАВКА КОМАНД КОПИРОВАНИЯ НА GPU
        auto [allocateCmdRes, tempCmdBuffers] = device.allocateCommandBuffersUnique({
            .commandPool = transferCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        });
        if (allocateCmdRes != vk::Result::eSuccess) return {allocateCmdRes, {}};

        vk::CommandBuffer cmd = tempCmdBuffers[0].get();
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        stagingMeshInfo.recordCopyCommandsToBuffer(
            cmd,
            *resultData.vertexBuffer,
            *resultData.indexBuffer,
            *resultData.indirectDrawBuffer,
            *resultData.modelSsbo
        );

        for (uint32_t i = 0; i < matCount; ++i) {
            stagingMatInfos[i].prepareCopyCommandsToBuffer(
                cmd,
                *stagingBuffer,
                *resultData.materials[i].deviceMaterialInfo.uniformBufferMaterialProperties,
                *resultData.materials[i].deviceMaterialInfo.albedoImage,
                *resultData.materials[i].deviceMaterialInfo.normalImage,
                *resultData.materials[i].deviceMaterialInfo.ormImage,
                *resultData.materials[i].deviceMaterialInfo.emissionImage,
                *resultData.materials[i].deviceMaterialInfo.heightImage
            );
            resultData.materials[i].deviceMaterialInfo.recordMaterialImagesBarriers(
                cmd,
                static_cast<uint32_t>(stagingMatInfos[i].albedoInfo.mipLevels.size()),
                static_cast<uint32_t>(stagingMatInfos[i].normalInfo.mipLevels.size()),
                static_cast<uint32_t>(stagingMatInfos[i].ormInfo.mipLevels.size()),
                static_cast<uint32_t>(stagingMatInfos[i].emissionInfo.mipLevels.size()),
                static_cast<uint32_t>(stagingMatInfos[i].heightInfo.mipLevels.size()));
        }

        cmd.end();

        vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd
        };

        if (auto submitRes = transferQueue.submit(1, &submitInfo, nullptr); submitRes != vk::Result::eSuccess) {
            return {submitRes, {}};
        }

        resultData.sceneLightingData = SceneLightingData{
            .ambient = hostSceneData.ambientLight,
            .directionalLightCount = 1,
            .pointLightCount = 0,
            .spotLightCount = 0
        };

        resultData.directionalLightDatas.push_back(
            {
                .direction = hostSceneData.sunLight.direction,
                .color = hostSceneData.sunLight.color,
                .lightSpaceMatrix = glm::mat4(1.0) // Обновляется при рендеринге
            }
        );

        transferQueue.waitIdle();

        return {vk::Result::eSuccess, std::move(resultData)};
    }

    void PbrRender::recordRenderFrameCommands(
        DeviceSceneData const& sceneData,
        vk::CommandBuffer cmd,
        FrameData const& frameData,
        RenderTargets const& targets,
        std::function<void(vk::CommandBuffer)> const& additionalCommands) const
    {
        // =========================================================================
        // ПАСС 1: РЕНДЕРИНГ В КАРТУ ТЕНЕЙ (SHADOW PASS)
        // =========================================================================
        {
            vk::ClearValue shadowClear{ .depthStencil = { .depth = 1.0f, .stencil = 0 } };

            vk::RenderPassBeginInfo shadowPassBegin{
                .renderPass = *shadowRenderPass,
                .framebuffer = *frameData.shadowRenderPassFramebuffer, // Берем актуальный FB из таргетов
                .renderArea = vk::Rect2D{ {0, 0}, frameData.shadowExtent },
                .clearValueCount = 1,
                .pClearValues = &shadowClear
            };

            cmd.beginRenderPass(shadowPassBegin, vk::SubpassContents::eInline);

            vk::Viewport viewport{
                .x = 0.0f, .y = 0.0f,
                .width = static_cast<float>(frameData.shadowExtent.width),
                .height = static_cast<float>(frameData.shadowExtent.height),
                .minDepth = 0.0f, .maxDepth = 1.0f
            };
            vk::Rect2D scissor{
                .offset = {0, 0},
                .extent = frameData.shadowExtent
            };

            cmd.setViewport(0, 1, &viewport);
            cmd.setScissor(0, 1, &scissor);
            cmd.setDepthBias(0.0f, 0.0f, 0.0f);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPipeline);

            std::array shadowDescriptorSets{
                sceneData.modelSsboDescriptorSet,
                frameData.sceneDataSet
            };

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *shadowPipelineLayout,
                0, shadowDescriptorSets,
                {}
            );

            uint32_t totalIndirectCount = 0;
            for (auto indirectDraw : sceneData.indirectDraws) {
                totalIndirectCount += indirectDraw.commandCount;
            }

            vk::Buffer vBuffer = *sceneData.vertexBuffer;
            vk::DeviceSize offset = sceneData.positionAttributeOffset;
            cmd.bindVertexBuffers(0, 1, &vBuffer, &offset);
            cmd.bindIndexBuffer(*sceneData.indexBuffer, sceneData.indexBufferOffset, vk::IndexType::eUint32);

            cmd.drawIndexedIndirect(
                *sceneData.indirectDrawBuffer,
                sceneData.indirectDrawBufferOffset,
                totalIndirectCount,
                sizeof(vk::DrawIndexedIndirectCommand)
            );

            cmd.endRenderPass();
        }

        // =========================================================================
        // ПАСС 2: ОСНОВНОЙ РЕНДЕР (MAIN PASS)
        // =========================================================================
        {
            std::array clearValues{
                vk::ClearValue{ .color = vk::ClearColorValue{.float32 = std::array{sceneData.sceneLightingData.ambient.r, sceneData.sceneLightingData.ambient.g, sceneData.sceneLightingData.ambient.b, 1.0f} } },
                vk::ClearValue{ .depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 } }
            };

            vk::RenderPassBeginInfo mainPassBegin{
                .renderPass = *mainRenderPass,
                .framebuffer = *targets.mainRenderPassFramebuffer, // Берем FB по индексу кадра
                .renderArea = vk::Rect2D{ {0, 0}, targets.renderTargetExtent },
                .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                .pClearValues = clearValues.data()
            };

            cmd.beginRenderPass(mainPassBegin, vk::SubpassContents::eInline);

            vk::Viewport viewport{
                .x = 0.0f, .y = 0.0f,
                .width = static_cast<float>(targets.renderTargetExtent.width),
                .height = static_cast<float>(targets.renderTargetExtent.height),
                .minDepth = 0.0f, .maxDepth = 1.0f
            };
            vk::Rect2D scissor{
                .offset = {0, 0},
                .extent = targets.renderTargetExtent
            };

            cmd.setViewport(0, 1, &viewport);
            cmd.setScissor(0, 1, &scissor);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mainPipeline);

            std::array mainRenderingSets {
                samplersSet,     // Будет доступен как set = 0
                sceneData.modelSsboDescriptorSet,        // Будет доступен как set = 1
                frameData.sceneDataSet // Будет доступен как set = 2
            };

            // Биндим Global Scene Set
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *mainPipelineLayout,
                0, mainRenderingSets,
                {}
            );

            std::array<vk::Buffer, 2> vBuffers{ *sceneData.vertexBuffer, *sceneData.vertexBuffer };
            std::array vOffsets{ sceneData.positionAttributeOffset, sceneData.normalUvTangentAttributeOffset };
            cmd.bindVertexBuffers(0, 2, vBuffers.data(), vOffsets.data());
            cmd.bindIndexBuffer(*sceneData.indexBuffer, sceneData.indexBufferOffset, vk::IndexType::eUint32);

            // Рисуем все материалы
            for (const auto& indirectDraw : sceneData.indirectDraws) {
                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    *mainPipelineLayout,
                    3, 1, &sceneData.materials[indirectDraw.materialIndex].materialSet,
                    0, nullptr
                );

                cmd.drawIndexedIndirect(
                    *sceneData.indirectDrawBuffer,
                    indirectDraw.indirectBufferOffset,
                    indirectDraw.commandCount,
                    sizeof(vk::DrawIndexedIndirectCommand)
                );
            }

            additionalCommands(cmd);

            cmd.endRenderPass();
        }
    }

    glm::mat4 calculateLightSpaceMatrix(
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::vec3& lightDir,
        float shadowMapResolution) // Передайте сюда размер карты теней, например, 2048.0f
    {
        // Шаг 1: Получаем вершины Frustum камеры в мировых координатах
        glm::mat4 inv = glm::inverse(projMatrix * viewMatrix);
        std::vector<glm::vec4> frustumCorners;
        for (unsigned int x = 0; x < 2; ++x) {
            for (unsigned int y = 0; y < 2; ++y) {
                for (unsigned int z = 0; z < 2; ++z) {
                    glm::vec4 pt = inv * glm::vec4(
                        2.0f * static_cast<float>(x) - 1.0f,
                        2.0f * static_cast<float>(y) - 1.0f,
                        static_cast<float>(z),
                        1.0f
                    );
                    frustumCorners.push_back(pt / pt.w);
                }
            }
        }

        // Шаг 2: Находим геометрический центр Frustum
        glm::vec3 center{0.0f};
        for (const auto& worldCorner : frustumCorners) {
            center += glm::vec3(worldCorner);
        }
        center /= static_cast<float>(frustumCorners.size());

        // СТАБИЛИЗАЦИЯ ШАГ 1: Считаем радиус ограничивающей сферы Frustum.
        // Вместо динамического AABB мы берем максимальное расстояние от центра до угла frustum.
        // Это делает размеры проекции света константными при вращении камеры.
        float sphereRadius = 0.0f;
        for (const auto& corner : frustumCorners) {
            float dist = glm::length(glm::vec3(corner) - center);
            sphereRadius = std::max(sphereRadius, dist);
        }
        // Округляем радиус с небольшим запасом
        sphereRadius = std::ceil(sphereRadius * 1.1f);

        // Отодвигаем источник света далеко назад на основе фиксированного радиуса
        glm::vec3 normalizedLightDir = glm::normalize(lightDir);
        glm::vec3 lightPos = center - (normalizedLightDir * sphereRadius * 2.0f);

        glm::mat4 lightView = glm::lookAt(
            lightPos,
            center,
            glm::vec3{0.0f, 1.0f, 0.0f}
        );

        // Изначальные жесткие границы на основе сферы (они симметричны и неизменны)
        float minX = -sphereRadius;
        float maxX =  sphereRadius;
        float minY = -sphereRadius;
        float maxY =  sphereRadius;

        // СТАБИЛИЗАЦИЯ ШАГ 2: Привязка к пиксельной сетке (Texel Snapping).
        // Находим, сколько мировых единиц приходится на один пиксель текстуры тени
        float worldTexelSize = (maxX - minX) / shadowMapResolution;

        // Переводим центр в пространство света, чтобы округлить его координаты
        glm::vec4 lightSpaceCenter = lightView * glm::vec4(center, 1.0f);

        // Округляем координаты до ближайшего текселя
        lightSpaceCenter.x = std::floor(lightSpaceCenter.x / worldTexelSize) * worldTexelSize;
        lightSpaceCenter.y = std::floor(lightSpaceCenter.y / worldTexelSize) * worldTexelSize;

        // Восстанавливаем скорректированную матрицу lightView с учетом округленного центра
        // Это убирает микро-дрожание при плавном перемещении камеры
        glm::vec3 snappedCenter = glm::vec3(glm::inverse(lightView) * lightSpaceCenter);
        lightView = glm::lookAt(snappedCenter - (normalizedLightDir * sphereRadius * 2.0f), snappedCenter, glm::vec3{0.0f, 1.0f, 0.0f});

        // Для Z-плоскостей оставляем надежный глубокий диапазон
        float minZ = -sphereRadius * 10.0f;
        float maxZ =  sphereRadius * 10.0f;

        // Строим итоговую ортографическую проекцию без ручных инверсий
        glm::mat4 lightProjection = glm::orthoLH_ZO(minX, maxX, minY, maxY, minZ, maxZ);

        return lightProjection * lightView;
    }

    // =========================================================================
    // НОВЫЙ МЕТОД: Умное обновление данных камеры и теней на GPU
    // =========================================================================
    vk::Result PbrRender::updateSceneData(
        resources::DeviceAllocator const& allocator,
        DeviceSceneData& sceneData,
        FrameData& frameData,
        glm::mat4 const& viewMatrix,
        glm::mat4 const& projectionMatrix,
        glm::mat4 const& shortProjectionMatrix,
        glm::vec3 const& cameraPos)
    {

        glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix(viewMatrix, shortProjectionMatrix, sceneData.directionalLightDatas[0].direction, 4096.0f);

        DirectionalLightData sunLightData{
            .direction = sceneData.directionalLightDatas[0].direction,
            .color = sceneData.directionalLightDatas[0].color,
            .lightSpaceMatrix = lightSpaceMatrix
        };

        sceneData.directionalLightDatas[0] = sunLightData;

        // 2. Обновляем UBO камеры (Set 0, Binding 0)
        CameraUniformData cameraData{
            .viewProj = projectionMatrix * viewMatrix,
            .cameraPos = cameraPos
        };

        auto writeSceneLightDataResult = allocator.writeBufferFromHost({
            .dstBuffer = *frameData.lightInfoUbo,
            .dstBufferOffset = 0,
            .srcData = &sceneData.sceneLightingData,
            .dataSize = sizeof(SceneLightingData)
        });

        if (writeSceneLightDataResult != vk::Result::eSuccess) return writeSceneLightDataResult;

        auto writeDirectionalLightDataResult = allocator.writeBufferFromHost({
            .dstBuffer = *frameData.lightSsbo,
            .dstBufferOffset = 0,
            .srcData = sceneData.directionalLightDatas.data(),
            .dataSize = sizeof(DirectionalLightData)
        });

        if (writeDirectionalLightDataResult != vk::Result::eSuccess) return writeDirectionalLightDataResult;

        auto writeCameraDataResult = allocator.writeBufferFromHost({
            .dstBuffer = *frameData.cameraUbo,
            .dstBufferOffset = 0,
            .srcData = &cameraData,
            .dataSize = sizeof(CameraUniformData)
        });

        if (writeCameraDataResult != vk::Result::eSuccess) return writeCameraDataResult;

        return vk::Result::eSuccess;
    }

    vk::ResultValue<std::vector<RenderTargets>> PbrRender::createRenderTargets(
        vk::Device device,
        resources::DeviceAllocator const &allocator,
        std::vector<vk::Image> const &targetImages,
        vk::Extent2D renderTargetExtent) const{

        std::vector<RenderTargets> result;
        result.reserve(targetImages.size());

        for (auto const& target : targetImages) {
            auto [depthBufferCreateResult, uniqueDepthBuffer] = allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .extent = vk::Extent3D{
                        .width = renderTargetExtent.width,
                        .height = renderTargetExtent.height,
                        .depth = 1
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eGpuOnly
            );

            if (depthBufferCreateResult != vk::Result::eSuccess) return {depthBufferCreateResult, {}};

            auto [colorImageViewCreateResult, uniqueColorImageView] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = target,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eB8G8R8A8Srgb,
                    .components = {
                        .r = vk::ComponentSwizzle::eIdentity,
                        .g = vk::ComponentSwizzle::eIdentity,
                        .b = vk::ComponentSwizzle::eIdentity,
                        .a = vk::ComponentSwizzle::eIdentity
                    },
                    .subresourceRange = vk::ImageSubresourceRange{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );

            if (colorImageViewCreateResult != vk::Result::eSuccess) return {colorImageViewCreateResult, {}};

            auto [depthBufferImageViewCreateResult, uniqueDepthBufferImageView] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = *uniqueDepthBuffer,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .components = {
                        .r = vk::ComponentSwizzle::eIdentity,
                        .g = vk::ComponentSwizzle::eIdentity,
                        .b = vk::ComponentSwizzle::eIdentity,
                        .a = vk::ComponentSwizzle::eIdentity
                    },
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );

            if (depthBufferImageViewCreateResult != vk::Result::eSuccess) return {depthBufferImageViewCreateResult, {}};

            std::array attachments {
                *uniqueColorImageView,
                *uniqueDepthBufferImageView
            };

            auto [framebufferCreateResult, uniqueFramebuffer] = device.createFramebufferUnique(
                vk::FramebufferCreateInfo{
                    .renderPass = *mainRenderPass,
                    .attachmentCount = 2,
                    .pAttachments = attachments.data(),
                    .width = renderTargetExtent.width,
                    .height = renderTargetExtent.height,
                    .layers = 1
                }
            );

            if (framebufferCreateResult != vk::Result::eSuccess) return {framebufferCreateResult, {}};

            result.emplace_back(RenderTargets{
                .depthBufferImage = std::move(uniqueDepthBuffer),
                .depthBufferImageView = std::move(uniqueDepthBufferImageView),
                .colorAttachmentImageView = std::move(uniqueColorImageView),
                .mainRenderPassFramebuffer = std::move(uniqueFramebuffer),
                .renderTargetExtent = renderTargetExtent
            });
        }
        return {vk::Result::eSuccess, std::move(result)};
    }

    vk::ResultValue<std::vector<FrameData>> PbrRender::createFrameDatas(
        vk::Device device,
        resources::DeviceAllocator const &allocator,
        vk::Extent2D shadowMapExtent,
        vk::DescriptorPool descriptorPool,
        uint32_t frameCount
    ) const {
        std::vector<FrameData> result;
        result.reserve(frameCount);

        std::vector layouts{frameCount, *pbrSceneDataSetLayout};

        auto [createSceneSetsResult, uniqueSceneSets] = device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo{
                .descriptorPool = descriptorPool,
                .descriptorSetCount = frameCount,
                .pSetLayouts = layouts.data()
            }
        );

        if (createSceneSetsResult != vk::Result::eSuccess) return {createSceneSetsResult, {}};

        for (uint32_t i = 0; i < frameCount; i++) {
            auto [shadowMapImageCreateResult, uniqueShadowMapImage] = allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .extent = {
                        .width = shadowMapExtent.width,
                        .height = shadowMapExtent.height,
                        .depth = 1
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eGpuOnly
            );

            if (shadowMapImageCreateResult != vk::Result::eSuccess) return {shadowMapImageCreateResult, {}};

            auto [shadowMapImageViewCreateResult, uniqueShadowMapImageView] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = *uniqueShadowMapImage,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .components = {
                        .r = vk::ComponentSwizzle::eIdentity,
                        .g = vk::ComponentSwizzle::eIdentity,
                        .b = vk::ComponentSwizzle::eIdentity,
                        .a = vk::ComponentSwizzle::eIdentity
                    },
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );

            if (shadowMapImageViewCreateResult != vk::Result::eSuccess) return {shadowMapImageViewCreateResult, {}};

            std::array attachments {
                *uniqueShadowMapImageView
            };

            auto [framebufferCreateResult, uniqueFramebuffer] = device.createFramebufferUnique(
                vk::FramebufferCreateInfo{
                    .renderPass = *shadowRenderPass,
                    .attachmentCount = 1,
                    .pAttachments = attachments.data(),
                    .width = shadowMapExtent.width,
                    .height = shadowMapExtent.height,
                    .layers = 1
                }
            );

            if (framebufferCreateResult != vk::Result::eSuccess) return {framebufferCreateResult, {}};

            auto [lightSceneDataUboCreateResult, uniqueLightSceneDataUbo] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(SceneLightingData),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlags {
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped)
                }
            );

            if (lightSceneDataUboCreateResult != vk::Result::eSuccess) return {lightSceneDataUboCreateResult, {}};

            auto [lightSsboCreateResult, uniqueLightSsbo] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(DirectionalLightData),
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlags {
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped)
                }
            );

            if (lightSsboCreateResult != vk::Result::eSuccess) return {lightSsboCreateResult, {}};

            auto [cameraUboCreateResult, uniqueCameraUbo] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(CameraUniformData),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlags {
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped)
                }
            );

            if (cameraUboCreateResult != vk::Result::eSuccess) return {cameraUboCreateResult, {}};

            vk::DescriptorBufferInfo cameraUboDescriptorInfo {
                .buffer = *uniqueCameraUbo,
                .offset = 0,
                .range = sizeof(CameraUniformData)
            };

            vk::DescriptorBufferInfo sceneLightDataUboDescriptorInfo {
                .buffer = *uniqueLightSceneDataUbo,
                .offset = 0,
                .range = sizeof(SceneLightingData)
            };

            vk::DescriptorBufferInfo lightSsboDescriptorInfo {
                .buffer = *uniqueLightSsbo,
                .offset = 0,
                .range = sizeof(DirectionalLightData)
            };

            vk::DescriptorImageInfo shadowMapImageDescriptorInfo {
                .imageView = *uniqueShadowMapImageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            };

            std::array sceneDataDescriptorWrites {
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &cameraUboDescriptorInfo
                },
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &sceneLightDataUboDescriptorInfo
                },
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &lightSsboDescriptorInfo
                },
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 3,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eSampledImage,
                    .pImageInfo = &shadowMapImageDescriptorInfo
                }
            };

            device.updateDescriptorSets(
                sceneDataDescriptorWrites,
                {}
            );

            result.emplace_back(
                FrameData{
                    .shadowMapImage = std::move(uniqueShadowMapImage),
                    .shadowMapImageView = std::move(uniqueShadowMapImageView),
                    .shadowRenderPassFramebuffer = std::move(uniqueFramebuffer),
                    .shadowExtent = shadowMapExtent,
                    .cameraUbo = std::move(uniqueCameraUbo),
                    .lightInfoUbo = std::move(uniqueLightSceneDataUbo),
                    .lightSsbo = std::move(uniqueLightSsbo),
                    .sceneDataSet = std::move(uniqueSceneSets[i]),
                }
            );
        }
        return {vk::Result::eSuccess, std::move(result)};
    }
}