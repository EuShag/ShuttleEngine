//
// Created by Shagu on 14.09.2026.
//
#include "EngineImpl.hpp"
#include "Render/SceneData.hpp"

namespace shuttle::engine {

    namespace {
        void* getDestinationAddress(void* ptr, vk::DeviceSize offset = 0)
        {
            return static_cast<char*>(ptr) + offset;
        }
    }

    using namespace render;

    vk::ResultValue<SceneHandle> EngineImpl::uploadScene(
        const LoadedSceneData& loadedSceneData)
    {

        std::vector<resources::UniqueAllocatedBuffer> stagingBuffers;

        auto [allocateCommandBufferResult, cmdUnique] = allocateOneTimeCommandBuffer();
        if (allocateCommandBufferResult != vk::Result::eSuccess) return {allocateCommandBufferResult, {}};
        vk::CommandBuffer cmd = *cmdUnique;

        std::vector<DirectionalLightGpuInfo> directionalLightGpu(loadedSceneData.directionalLights.empty() ? 1 : loadedSceneData.directionalLights.size());
        if (!loadedSceneData.directionalLights.empty()) {
            for (size_t i = 0; i < loadedSceneData.directionalLights.size(); ++i) {
                auto const& lightInfo = loadedSceneData.directionalLights[i];
                directionalLightGpu[i] = {
                    .lightDirection = glm::vec4{lightInfo.directionAndIntensity.x, lightInfo.directionAndIntensity.y, lightInfo.directionAndIntensity.z, 1.0f},
                    .lightColorAndIntensity = glm::vec4{lightInfo.color.x, lightInfo.color.y, lightInfo.color.z, lightInfo.directionAndIntensity.w}
                };
            }
        } else {
            directionalLightGpu[0] = {
                .lightDirection = glm::vec4{0.5f, 0.5f, - 0.5f, 0.0f},
                .lightColorAndIntensity = glm::vec4{1.0f, 1.0f, 1.0f, 100.0f},
            };
        }

        const size_t nodeSectionSize = loadedSceneData.nodes.size_bytes();
        const size_t levelSectionSize = loadedSceneData.levels.size_bytes();
        const size_t transformSectionSize = loadedSceneData.transforms.size_bytes();
        const size_t drawablesSectionSize = loadedSceneData.drawables.size_bytes();
        const size_t positionSectionSize = loadedSceneData.positions.size_bytes();
        const size_t attributeSectionSize = loadedSceneData.attributes.size_bytes();
        const size_t indexSectionSize = loadedSceneData.indices.size_bytes();
        const size_t directionalLightSectionSize = directionalLightGpu.size() * sizeof(DirectionalLightGpuInfo);
        const size_t materialSectionSize = loadedSceneData.materials.size() * sizeof(MaterialGpuInfo);
        const size_t meshSectionSize = loadedSceneData.meshes.size() * sizeof(MeshGpuInfo);

        HostSceneData hostSceneData{};
        hostSceneData.nodes.assign(loadedSceneData.nodes.begin(), loadedSceneData.nodes.end());
        hostSceneData.levels.assign(loadedSceneData.levels.begin(), loadedSceneData.levels.end());
        hostSceneData.transforms.assign(loadedSceneData.transforms.begin(), loadedSceneData.transforms.end());
        hostSceneData.drawableObjects.assign(loadedSceneData.drawables.begin(), loadedSceneData.drawables.end());
        hostSceneData.directionalLights.assign(loadedSceneData.directionalLights.begin(), loadedSceneData.directionalLights.end());

        SceneFrameRequirements sceneFrameRequirements{};
        sceneFrameRequirements.transformCount = static_cast<uint32_t>(loadedSceneData.transforms.size());
        sceneFrameRequirements.drawableObjectCount = static_cast<uint32_t>(loadedSceneData.drawables.size());
        sceneFrameRequirements.meshCount = static_cast<uint32_t>(loadedSceneData.meshes.size());

        std::vector<Texture> textures;
        textures.reserve(loadedSceneData.textureMetadatas.size());

        for (const auto& textureMetadata : loadedSceneData.textureMetadatas)
        {
            if (textureMetadata.mipCount == 0 || textureMetadata.width == 0 || textureMetadata.height == 0 ||
                textureMetadata.depth != 1 || textureMetadata.layerCount != 1 ||
                textureMetadata.imageType != assets::formats::texture::ImageType::Image2D ||
                textureMetadata.imageViewType != assets::formats::texture::ImageViewType::View2D)
            {
                return {vk::Result::eErrorInitializationFailed, {}};
            }

            auto [createTextureResult, texture] = createTexture(
                static_cast<vk::Format>(textureMetadata.format),
                vk::ImageViewType::e2D,
                {.width = textureMetadata.width, .height = textureMetadata.height, .depth = 1},
                textureMetadata.mipCount,
                1
            );

            textures.emplace_back(std::move(texture));
        }

        auto [createSceneInfoBufferResult, sceneInfoBuffer] = createShaderDeviceAddressBuffer(sizeof(SceneGpuInfo));
        auto [createMeshBufferResult, meshBuffer] = createShaderDeviceAddressBuffer(meshSectionSize);
        auto [createPositionBufferResult, positionBuffer] = createShaderDeviceAddressBuffer(positionSectionSize);
        auto [createAttributeBufferResult, attributeBuffer] = createShaderDeviceAddressBuffer(attributeSectionSize);
        auto [createIndexBufferResult, indexBuffer] = createIndexBuffer(indexSectionSize);
        auto [createMaterialBufferResult, materialBuffer] = createShaderDeviceAddressBuffer(materialSectionSize);
        auto [createNodeBufferResult, nodeBuffer] = createShaderDeviceAddressBuffer(nodeSectionSize);
        auto [createLevelBufferResult, levelBuffer] = createShaderDeviceAddressBuffer(levelSectionSize);
        auto [createTransformBufferResult, transformBuffer] = createShaderDeviceAddressBuffer(transformSectionSize);
        auto [createDrawableBufferResult, drawableBuffer] = createShaderDeviceAddressBuffer(drawablesSectionSize);
        auto [createDirectionalLightBufferResult, directionalLightBuffer] = createShaderDeviceAddressBuffer(directionalLightSectionSize);

        if (createSceneInfoBufferResult != vk::Result::eSuccess) return {createSceneInfoBufferResult, {}};
        if (createMeshBufferResult != vk::Result::eSuccess) return {createMeshBufferResult, {}};
        if (createPositionBufferResult != vk::Result::eSuccess) return {createPositionBufferResult, {}};
        if (createAttributeBufferResult != vk::Result::eSuccess) return {createAttributeBufferResult, {}};
        if (createIndexBufferResult != vk::Result::eSuccess) return {createIndexBufferResult, {}};
        if (createMaterialBufferResult != vk::Result::eSuccess) return {createMaterialBufferResult, {}};
        if (createNodeBufferResult != vk::Result::eSuccess) return {createNodeBufferResult, {}};
        if (createLevelBufferResult != vk::Result::eSuccess) return {createLevelBufferResult, {}};
        if (createTransformBufferResult != vk::Result::eSuccess) return {createTransformBufferResult, {}};
        if (createDrawableBufferResult != vk::Result::eSuccess) return {createDrawableBufferResult, {}};
        if (createDirectionalLightBufferResult != vk::Result::eSuccess) return {createDirectionalLightBufferResult, {}};

        constexpr vk::DeviceSize Alignment = 16;
        auto alignUp = [](vk::DeviceSize value, vk::DeviceSize alignment)
        { return (value + alignment - 1) & ~(alignment - 1); };

        SceneUploadOffsets offsets{};
        vk::DeviceSize cursor = 0;
        offsets.sceneInfo = cursor;         cursor += sizeof(SceneGpuInfo);        cursor = alignUp(cursor, Alignment);
        offsets.nodes = cursor;             cursor += nodeSectionSize;             cursor = alignUp(cursor, Alignment);
        offsets.levels = cursor;            cursor += levelSectionSize;            cursor = alignUp(cursor, Alignment);
        offsets.transforms = cursor;        cursor += transformSectionSize;        cursor = alignUp(cursor, Alignment);
        offsets.drawables = cursor;         cursor += drawablesSectionSize;        cursor = alignUp(cursor, Alignment);
        offsets.meshes = cursor;            cursor += meshSectionSize;             cursor = alignUp(cursor, Alignment);
        offsets.positions = cursor;         cursor += positionSectionSize;         cursor = alignUp(cursor, Alignment);
        offsets.attributes = cursor;        cursor += attributeSectionSize;        cursor = alignUp(cursor, Alignment);
        offsets.indices = cursor;           cursor += indexSectionSize;            cursor = alignUp(cursor, Alignment);
        offsets.materials = cursor;         cursor += materialSectionSize;         cursor = alignUp(cursor, Alignment);
        offsets.directionalLights = cursor; cursor += directionalLightSectionSize; cursor = alignUp(cursor, Alignment);

        for (size_t i = 0; i < loadedSceneData.textureMetadatas.size(); ++i)
        {
            if (loadedSceneData.textureMetadatas[i].mipTableOffset % sizeof(assets::formats::texture::TextureMipMetadata) != 0)
            {
                return {vk::Result::eErrorInitializationFailed, {}};
            }
            const uint64_t firstMipIndex = loadedSceneData.textureMetadatas[i].mipTableOffset / sizeof(assets::formats::texture::TextureMipMetadata);
            const auto mipCount = static_cast<uint64_t>(loadedSceneData.textureMetadatas[i].mipCount);
            if (firstMipIndex + mipCount > loadedSceneData.textureMipMetadatas.size())
            {
                return {vk::Result::eErrorInitializationFailed, {}};
            }

            auto textureMipDatas = std::span(
                loadedSceneData.textureMipMetadatas.data() +
                loadedSceneData.textureMetadatas[i].mipTableOffset / sizeof(assets::formats::texture::TextureMipMetadata),
                loadedSceneData.textureMetadatas[i].mipCount
            );

            for (size_t j = 0; j < textureMipDatas.size(); ++j)
            {
                const auto& textureMipMetadata = textureMipDatas[j];

                ImageMipUploadInfo textureMipUploadInfo{};
                textureMipUploadInfo.imageIndex = static_cast<uint32_t>(i);
                textureMipUploadInfo.mipLevel = static_cast<uint32_t>(j);
                textureMipUploadInfo.sourceOffset = textureMipMetadata.dataOffset;
                textureMipUploadInfo.stagingOffset = cursor;
                textureMipUploadInfo.width = textureMipMetadata.width;
                textureMipUploadInfo.height = textureMipMetadata.height;
                textureMipUploadInfo.dataSize = textureMipMetadata.dataSize;
                if (textureMipUploadInfo.width == 0 || textureMipUploadInfo.height == 0)
                {
                    return {vk::Result::eErrorInitializationFailed, {}};
                }
                if (textureMipUploadInfo.sourceOffset + textureMipUploadInfo.dataSize > loadedSceneData.textureBytes.size())
                {
                    return {vk::Result::eErrorInitializationFailed, {}};
                }
                offsets.textureMipUploadInfos.push_back(textureMipUploadInfo);
                cursor += textureMipMetadata.dataSize;
                cursor = alignUp(cursor, Alignment);
            }
        }

        offsets.totalSize = cursor;

        auto [stagingBufferCreateResult, stagingBuffer] = createStagingBuffer(offsets.totalSize);
        if (stagingBufferCreateResult != vk::Result::eSuccess) return {stagingBufferCreateResult, {}};

        void* stagingBufferData = m_allocator->getMappedPointer(*stagingBuffer);

        auto copyToStaging = [stagingBufferData](vk::DeviceSize dstOffset, const void* src, size_t size)
        {
            if (size == 0) return;
            std::memcpy(getDestinationAddress(stagingBufferData, dstOffset), src, size);
        };

        SceneGpuInfo sceneGpuInfo {
            .sceneNodesBufferDeviceAddress = m_device.getBufferAddress({.buffer = *nodeBuffer}, m_dispatcher),
            .materialDatasBufferAddress = m_device.getBufferAddress({.buffer = *materialBuffer}, m_dispatcher),
            .lightDatasBufferAddress = m_device.getBufferAddress({.buffer = *directionalLightBuffer}, m_dispatcher),
            .meshDatasBufferAddress = m_device.getBufferAddress({.buffer = *meshBuffer}, m_dispatcher),
            .drawablesBufferAddress = m_device.getBufferAddress({.buffer = *drawableBuffer}, m_dispatcher),
            .localTransformsBufferAddress = m_device.getBufferAddress({.buffer = *transformBuffer}, m_dispatcher),

            .materialCount = static_cast<uint32_t>(loadedSceneData.materials.size()),
            .lightCount = static_cast<uint32_t>(directionalLightGpu.size()),
            .meshCount = static_cast<uint32_t>(loadedSceneData.meshes.size()),
            .drawableCount = static_cast<uint32_t>(loadedSceneData.drawables.size()),
            .nodeCount = static_cast<uint32_t>(loadedSceneData.nodes.size()),
            .padding0 = 0
        };

        auto fallbackTextureIndices = m_fallbackTextures.indices;

        std::vector<MaterialGpuInfo> materialGpu(loadedSceneData.materials.size());
        for (size_t i = 0; i < loadedSceneData.materials.size(); ++i) {

            auto const& materialInfo = loadedSceneData.materials[i];

            uint32_t albedoIndex = materialInfo.albedoTexture == assets::formats::material::InvalidTextureIndex ||
                materialInfo.albedoTexture >= textures.size()
                ? fallbackTextureIndices.albedo
                : textures[materialInfo.albedoTexture].descriptorSlot.get();
            uint32_t normalIndex = materialInfo.normalTexture == assets::formats::material::InvalidTextureIndex ||
                materialInfo.normalTexture >= textures.size()
                ? fallbackTextureIndices.normal
                : textures[materialInfo.normalTexture].descriptorSlot.get();
            uint32_t ormIndex = materialInfo.ormTexture == assets::formats::material::InvalidTextureIndex ||
                materialInfo.ormTexture >= textures.size()
                ? fallbackTextureIndices.orm
                : textures[materialInfo.ormTexture].descriptorSlot.get();
            uint32_t emissiveIndex = materialInfo.emissiveTexture == assets::formats::material::InvalidTextureIndex ||
                materialInfo.emissiveTexture >= textures.size()
                ? fallbackTextureIndices.emission
                : textures[materialInfo.emissiveTexture].descriptorSlot.get();

            materialGpu[i] = {
                .baseColorFactor = materialInfo.baseColorFactor,
                .emissiveFactor = materialInfo.emissiveFactor,
                .metallicFactor = materialInfo.metallicFactor,
                .roughnessFactor = materialInfo.roughnessFactor,
                .alphaCutoff = materialInfo.alphaCutoff,
                .occlusionStrength = materialInfo.occlusionStrength,

                .emissiveStrength = materialInfo.emissiveStrength,

                .albedoTexture = albedoIndex,
                .normalTexture = normalIndex,
                .ormTexture = ormIndex,
                .emissiveTexture = emissiveIndex,

                .flags = materialInfo.flags,
                .pipelineFlags = materialInfo.pipelineFlags,
                .alphaMode = materialInfo.alphaMode
            };
        }

        std::vector<MeshGpuInfo> meshGpu(loadedSceneData.meshes.size());
        for (size_t i = 0; i < loadedSceneData.meshes.size(); ++i) {

            auto const& meshInfo = loadedSceneData.meshes[i];
            auto boundingSphere = meshInfo.boundingSphere;

            meshGpu[i] = {
                .positionAttributeBufferAddress =
                    m_device.getBufferAddress({.buffer = *positionBuffer}, m_dispatcher) +
                    meshInfo.positionOffset * sizeof(assets::formats::PositionAttribute),

                .normalUvTangentAttributeBufferAddress =
                    m_device.getBufferAddress({.buffer = *attributeBuffer}, m_dispatcher) +
                    meshInfo.attributeOffset * sizeof(assets::formats::VertexAttribute),

                .boundingSphere = glm::vec4{boundingSphere.center.x, boundingSphere.center.y, boundingSphere.center.z, boundingSphere.radius},
                .minAABB = meshInfo.localBounds.min,
                .maxAABB = meshInfo.localBounds.max,
                .meshFlags = meshInfo.meshFlags,
                .lodCount = meshInfo.lodCount
            };
            for (size_t j = 0; j < assets::formats::geometry::MaxMeshLods; ++j) {
                meshGpu[i].lods[j] = {
                    .firstIndex = meshInfo.lods[j].firstIndex,
                    .indexCount = meshInfo.lods[j].indexCount,
                    .geometricError = meshInfo.lods[j].geometricError,
                    .screenThreshold = meshInfo.lods[j].screenThreshold
                };
            }
        }

        copyToStaging(offsets.sceneInfo, &sceneGpuInfo, sizeof(SceneGpuInfo));
        copyToStaging(offsets.nodes, loadedSceneData.nodes.data(), nodeSectionSize);
        copyToStaging(offsets.levels, loadedSceneData.levels.data(), levelSectionSize);
        copyToStaging(offsets.transforms, loadedSceneData.transforms.data(), transformSectionSize);
        copyToStaging(offsets.drawables, loadedSceneData.drawables.data(), drawablesSectionSize);
        copyToStaging(offsets.meshes, meshGpu.data(), meshSectionSize);
        copyToStaging(offsets.positions, loadedSceneData.positions.data(), positionSectionSize);
        copyToStaging(offsets.attributes, loadedSceneData.attributes.data(), attributeSectionSize);
        copyToStaging(offsets.indices, loadedSceneData.indices.data(), indexSectionSize);
        copyToStaging(offsets.materials, materialGpu.data(), materialSectionSize);
        copyToStaging(offsets.directionalLights, directionalLightGpu.data(), directionalLightSectionSize);

        for (const auto& mip : offsets.textureMipUploadInfos)
        {
            std::memcpy(
                getDestinationAddress(stagingBufferData, mip.stagingOffset),
                loadedSceneData.textureBytes.data() + mip.sourceOffset,
                mip.dataSize
            );
        }

        // Запись команд в ресурсы сцены
        if (auto beginCommandBufferResult = cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, m_dispatcher);
            beginCommandBufferResult != vk::Result::eSuccess)
            return {beginCommandBufferResult, {}};

        std::vector<CopyBufferCommandInfo> copyBufferInfos{
            {.stagingOffset = offsets.sceneInfo, .destinationBuffer = *sceneInfoBuffer, .dataSize = sizeof(SceneGpuInfo)},
            {.stagingOffset = offsets.nodes, .destinationBuffer = *nodeBuffer, .dataSize = nodeSectionSize},
            {.stagingOffset = offsets.levels, .destinationBuffer = *levelBuffer, .dataSize = levelSectionSize},
            {.stagingOffset = offsets.transforms, .destinationBuffer = *transformBuffer, .dataSize = transformSectionSize},
            {.stagingOffset = offsets.drawables, .destinationBuffer = *drawableBuffer, .dataSize = drawablesSectionSize},
            {.stagingOffset = offsets.meshes, .destinationBuffer = *meshBuffer, .dataSize = meshSectionSize},
            {.stagingOffset = offsets.positions, .destinationBuffer = *positionBuffer, .dataSize = positionSectionSize},
            {.stagingOffset = offsets.attributes, .destinationBuffer = *attributeBuffer, .dataSize = attributeSectionSize},
            {.stagingOffset = offsets.indices, .destinationBuffer = *indexBuffer, .dataSize = indexSectionSize},
            {.stagingOffset = offsets.materials, .destinationBuffer = *materialBuffer, .dataSize = materialSectionSize},
            {.stagingOffset = offsets.directionalLights, .destinationBuffer = *directionalLightBuffer, .dataSize = directionalLightSectionSize},
        };

        std::vector<ImageBarrierInfo> textureBarrierInfos(loadedSceneData.textureMetadatas.size());
        for (size_t i = 0; i < loadedSceneData.textureMetadatas.size(); ++i)
        {
            textureBarrierInfos[i] = {.image = *textures[i].image, .mipLevels = loadedSceneData.textureMetadatas[i].mipCount};
        }
        std::vector<BufferBarrierInfo> bufferBarrierInfos{
            {.buffer = *nodeBuffer, .size = nodeSectionSize},
            {.buffer = *levelBuffer, .size = levelSectionSize},
            {.buffer = *transformBuffer, .size = transformSectionSize},
            {.buffer = *drawableBuffer, .size = drawablesSectionSize},
            {.buffer = *meshBuffer, .size = meshSectionSize},
            {.buffer = *positionBuffer, .size = positionSectionSize},
            {.buffer = *attributeBuffer, .size = attributeSectionSize},
            {.buffer = *indexBuffer, .size = indexSectionSize},
            {.buffer = *materialBuffer, .size = materialSectionSize},
            {.buffer = *directionalLightBuffer, .size = directionalLightSectionSize},
        };

        std::vector<vk::Image> images(textures.size());
        for (size_t i = 0; i < textures.size(); ++i)
        {
            images[i] = *textures[i].image;
        }

        writeCopyBufferCommand(cmd, *stagingBuffer, copyBufferInfos);
        writeImageSourceBarrier(cmd, textureBarrierInfos);
        writeCopyBufferImageCommands(cmd, *stagingBuffer, images, offsets.textureMipUploadInfos);
        writeBufferAndImageDestinationBarrier(cmd, textureBarrierInfos, bufferBarrierInfos);

        if (auto endCommandBufferResult = cmd.end(m_dispatcher); endCommandBufferResult != vk::Result::eSuccess)
            return {endCommandBufferResult, {}};

        auto [createFenceResult, fence] = m_device.createFenceUnique({}, nullptr, m_dispatcher);
        if (createFenceResult != vk::Result::eSuccess) return {createFenceResult, {}};

        vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
            .commandBuffer = cmd,
            .deviceMask = 0x1,
        };

        if (auto submitResult = m_graphicsQueue.submit2({
            {
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &commandBufferSubmitInfo
            }},
            *fence,
            m_dispatcher
        ); submitResult != vk::Result::eSuccess) return {submitResult, {}};

        if (auto waitFenceResult = m_device.waitForFences({*fence}, vk::True, UINT64_MAX, m_dispatcher);
            waitFenceResult != vk::Result::eSuccess)
            return {waitFenceResult, {}};

        DeviceSceneResources deviceResources{
            .sceneRootBuffer = std::move(sceneInfoBuffer),
            .positionBuffer = std::move(positionBuffer),
            .attributeBuffer = std::move(attributeBuffer),
            .indexBuffer = std::move(indexBuffer),
            .meshBuffer = std::move(meshBuffer),

            .materialBuffer = std::move(materialBuffer),

            .directionalLightBuffer = std::move(directionalLightBuffer),

            .textures = std::move(textures),

            .nodeBuffer = std::move(nodeBuffer),
            .levelBuffer = std::move(levelBuffer),
            .drawableBuffer = std::move(drawableBuffer),
            .transformBuffer = std::move(transformBuffer)
        };

        UploadSceneOutput sceneOutput {
            .hostSceneData = std::move(hostSceneData),
            .deviceSceneResources = std::move(deviceResources),
            .sceneFrameRequirements = sceneFrameRequirements
        };

        auto handle = m_uploadedScenes.allocate(std::move(sceneOutput));
        return {vk::Result::eSuccess, handle};
    }

}
