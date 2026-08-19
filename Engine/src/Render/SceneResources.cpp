//
// Created by Shagu on 27.07.2026.
//
//
// Created by Shagu on 27.07.2026.
//
#define GLM_ENABLE_EXPERIMENTAL
#include "Render.hpp"
#include "FallbackTextures.hpp"
#include "Assets/Core/BlobReader.hpp"
#include "IncludeVulkan.hpp"
#include "Assets/Formats/Material.hpp"
#include "Assets/Formats/Geometry.hpp"
#include "Assets/Formats/Texture.hpp"
#include <iostream>

#include "DescriptorHeapSet.hpp"
#include "SceneData.hpp"

namespace shuttle::engine::render {
    namespace {

        struct CopyBufferCommandInfo
        {
            vk::DeviceSize stagingOffset;
            vk::Buffer destinationBuffer;
            vk::DeviceSize dataSize;
        };

        void writeCopyBufferCommand(vk::CommandBuffer commandBuffer, vk::Buffer stagingBuffer,
                                    std::vector<CopyBufferCommandInfo> const& copyBufferInfos)
        {

            for (auto const& copyCommandBufferInfo : copyBufferInfos)
            {

                vk::BufferCopy2 region{
                    .srcOffset = copyCommandBufferInfo.stagingOffset, .dstOffset = 0, .size = copyCommandBufferInfo.dataSize};

                vk::CopyBufferInfo2 copyBufferInfo{.srcBuffer = stagingBuffer,
                                                   .dstBuffer = copyCommandBufferInfo.destinationBuffer,
                                                   .regionCount = 1,
                                                   .pRegions = &region};

                if (copyCommandBufferInfo.dataSize == 0) continue;
                commandBuffer.copyBuffer2(copyBufferInfo);
            }
        }

        struct TextureBarrierInfo
        {
            vk::Image image{};
            uint32_t mipLevels{};
        };

        struct BufferBarrierInfo
        {
            vk::Buffer buffer{};
            vk::DeviceSize size{};
        };

        void writeImageSourceBarrier(vk::CommandBuffer commandBuffer, std::vector<TextureBarrierInfo> const& textures)
        {
            std::vector<vk::ImageMemoryBarrier2> textureBarriers(textures.size());

            for (uint32_t i = 0; i < textureBarriers.size(); ++i)
            {
                textureBarriers[i] = {.srcStageMask = vk::PipelineStageFlagBits2::eNone,
                                      .srcAccessMask = vk::AccessFlagBits2::eNone,
                                      .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                                      .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                      .oldLayout = vk::ImageLayout::eUndefined,
                                      .newLayout = vk::ImageLayout::eTransferDstOptimal,
                                      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                                      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                                      .image = textures[i].image,
                                      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                           .baseMipLevel = 0,
                                                           .levelCount = textures[i].mipLevels,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1}};
            }

            commandBuffer.pipelineBarrier2({
                .imageMemoryBarrierCount = static_cast<uint32_t>(textureBarriers.size()),
                .pImageMemoryBarriers = textureBarriers.data(),
            });
        }

        void writeBufferAndImageDestinationBarrier(
            vk::CommandBuffer commandBuffer,
            std::vector<TextureBarrierInfo> const& textures,
            std::vector<BufferBarrierInfo> const& buffers)
        {

            std::vector<vk::ImageMemoryBarrier2> textureBarriers(textures.size());
            std::vector<vk::BufferMemoryBarrier2> bufferBarriers(buffers.size());

            for (uint32_t i = 0; i < textureBarriers.size(); ++i)
            {
                textureBarriers[i] = {
                    .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                    .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                    .dstAccessMask = vk::AccessFlagBits2::eNone,
                    .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                    .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = textures[i].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = textures[i].mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount = 1}};
            }
            for (uint32_t i = 0; i < buffers.size(); ++i)
            {
                bufferBarriers[i] = {
                    .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                    .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                    .dstAccessMask = vk::AccessFlagBits2::eNone,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .buffer = buffers[i].buffer,
                    .offset = 0,
                    .size = buffers[i].size,
                };
            }

            commandBuffer.pipelineBarrier2({
                .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
                .pBufferMemoryBarriers = bufferBarriers.data(),
                .imageMemoryBarrierCount = static_cast<uint32_t>(textureBarriers.size()),
                .pImageMemoryBarriers = textureBarriers.data(),
            });
        }

        struct TextureMipUploadInfo
        {
            uint32_t textureIndex{};

            uint32_t mipLevel{};

            vk::DeviceSize sourceOffset{};
            vk::DeviceSize stagingOffset{};

            uint32_t width{};
            uint32_t height{};

            vk::DeviceSize dataSize{};
        };

        void writeCopyBufferImageCommands(vk::CommandBuffer commandBuffer, vk::Buffer stagingBuffer,
                                          const std::vector<vk::Image>& images,
                                          std::vector<TextureMipUploadInfo> const& textureMipUploadInfos)
        {

            std::vector<std::vector<vk::BufferImageCopy2>> bufferImageCopies(images.size());
            std::vector<vk::CopyBufferToImageInfo2> copyBufferToImageInfos(images.size());

            for (auto const& textureMipUploadInfo : textureMipUploadInfos)
            {
                bufferImageCopies[textureMipUploadInfo.textureIndex].push_back(
                    {.bufferOffset = textureMipUploadInfo.stagingOffset,
                     .bufferRowLength = 0,
                     .bufferImageHeight = 0,
                     .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                          .mipLevel = textureMipUploadInfo.mipLevel,
                                          .baseArrayLayer = 0,
                                          .layerCount = 1},
                     .imageOffset = {.x = 0, .y = 0, .z = 0},
                     .imageExtent = {
                         .width = textureMipUploadInfo.width,
                         .height = textureMipUploadInfo.height,
                         .depth = 1,
                     }});
            }
            for (auto i = 0; i < copyBufferToImageInfos.size(); ++i)
            {

                copyBufferToImageInfos[i] =
                    vk::CopyBufferToImageInfo2{.srcBuffer = stagingBuffer,
                                               .dstImage = images[i],
                                               .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                                               .regionCount = static_cast<uint32_t>(bufferImageCopies[i].size()),
                                               .pRegions = bufferImageCopies[i].data()};
                if (copyBufferToImageInfos[i].regionCount == 0) continue;

                commandBuffer.copyBufferToImage2(copyBufferToImageInfos[i]);
            }
        }

        void* getDestinationAddress(void* ptr, vk::DeviceSize offset = 0)
        {
            return static_cast<char*>(ptr) + offset;
        }

        struct UploadOffsets
        {
            vk::DeviceSize sceneInfo{};
            vk::DeviceSize nodes{};
            vk::DeviceSize levels{};
            vk::DeviceSize transforms{};
            vk::DeviceSize drawables{};

            vk::DeviceSize meshes{};
            vk::DeviceSize positions{};
            vk::DeviceSize attributes{};
            vk::DeviceSize indices{};
            vk::DeviceSize materials{};

            vk::DeviceSize directionalLights{};

            std::vector<TextureMipUploadInfo> textureMipUploadInfos;

            vk::DeviceSize totalSize{};
        };

        vk::ResultValue<vk::UniqueCommandBuffer> allocateOneTimeCommandBuffer(vk::Device device, vk::CommandPool command_pool)
        {
            auto [allocateCommandBufferResult, uniqueCommandBuffers] = device.allocateCommandBuffersUnique(
                {.commandPool = command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1});
            return {allocateCommandBufferResult, std::move(uniqueCommandBuffers[0])};
        }

        vk::ResultValue<resources::UniqueAllocatedBuffer> createDeviceAddressBuffer(
            resources::DeviceAllocator const & allocator,
            vk::DeviceSize size)
        {
            return allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                .size = size == 0 ? 16 : size,
                .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive},
                resources::MemoryUsage::eGpuOnly);
        }

        vk::ResultValue<resources::UniqueAllocatedBuffer> createStagingBuffer(resources::DeviceAllocator const & allocator,
                                                                              vk::DeviceSize size)
        {
            return allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{.size = size,
                                     .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                     .sharingMode = vk::SharingMode::eExclusive},
                resources::MemoryUsage::eCpuToGpu, resources::AllocationCreateFlagBits::eMapped);
        }

        vk::ResultValue<resources::UniqueAllocatedBuffer> createIndexBuffer(
            resources::DeviceAllocator const & allocator,
            vk::DeviceSize size)
        {
            return allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{.size = size == 0 ? 16 : size,
                                     .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                              vk::BufferUsageFlagBits::eTransferDst,
                                     .sharingMode = vk::SharingMode::eExclusive},
                resources::MemoryUsage::eGpuOnly);
        }
    } // namespace

    vk::ResultValue<UploadSceneOutput> uploadScene(
        const LoadedSceneData& loadedSceneData,
        RenderContext& context,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        DescriptorHeapSet& descriptorHeapSet,
        FallbackTextureIndices const& fallbackTextureIndices)
    {

        std::vector<resources::UniqueAllocatedBuffer> stagingBuffers;

        auto [allocateCommandBufferResult, cmdUnique] = allocateOneTimeCommandBuffer(context.device, transferCommandPool);
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

            auto [createImageResult, image] = context.allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D,
                    .format = static_cast<vk::Format>(textureMetadata.format),
                    .extent = vk::Extent3D{textureMetadata.width, textureMetadata.height, 1},
                    .mipLevels = textureMetadata.mipCount,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                    .sharingMode = vk::SharingMode::eExclusive,
                    .initialLayout = vk::ImageLayout::eUndefined},
                resources::MemoryUsage::eGpuOnly);

            if (createImageResult != vk::Result::eSuccess) return {createImageResult, {}};

            auto [createImageViewResult, imageView] = context.device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = *image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = static_cast<vk::Format>(textureMetadata.format),
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = textureMetadata.mipCount,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );
            if (createImageViewResult != vk::Result::eSuccess) return {createImageViewResult, {}};

            auto [writeTexture2dDescriptorResult, textureDescriptor] = descriptorHeapSet.writeTextureUnique(*imageView);
            if (writeTexture2dDescriptorResult != vk::Result::eSuccess) return {writeTexture2dDescriptorResult, {}};

            textures.emplace_back(std::move(image), std::move(imageView), std::move(textureDescriptor));
        }

        auto [createSceneInfoBufferResult, sceneInfoBuffer] =
            createDeviceAddressBuffer(context.allocator, sizeof(SceneGpuInfo));

        auto [createMeshBufferResult, meshBuffer] =
            createDeviceAddressBuffer(context.allocator, meshSectionSize);

        auto [createPositionBufferResult, positionBuffer] =
            createDeviceAddressBuffer(context.allocator, positionSectionSize);

        auto [createAttributeBufferResult, attributeBuffer] =
            createDeviceAddressBuffer(context.allocator, attributeSectionSize);

        auto [createIndexBufferResult, indexBuffer] =
            createIndexBuffer(context.allocator, indexSectionSize);

        auto [createMaterialBufferResult, materialBuffer] =
            createDeviceAddressBuffer(context.allocator, materialSectionSize);

        auto [createNodeBufferResult, nodeBuffer] =
            createDeviceAddressBuffer(context.allocator, nodeSectionSize);

        auto [createLevelBufferResult, levelBuffer] =
            createDeviceAddressBuffer(context.allocator, levelSectionSize);

        auto [createTransformBufferResult, transformBuffer] =
            createDeviceAddressBuffer(context.allocator, transformSectionSize);

        auto [createDrawableBufferResult, drawableBuffer] =
            createDeviceAddressBuffer(context.allocator, drawablesSectionSize);

        auto [createDirectionalLightBufferResult, directionalLightBuffer] =
            createDeviceAddressBuffer(context.allocator, directionalLightSectionSize);

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

        UploadOffsets offsets{};
        vk::DeviceSize cursor = 0;
        offsets.sceneInfo = cursor;
        cursor += sizeof(SceneGpuInfo);
        cursor = alignUp(cursor, Alignment);
        offsets.nodes = cursor;
        cursor += nodeSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.levels = cursor;
        cursor += levelSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.transforms = cursor;
        cursor += transformSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.drawables = cursor;
        cursor += drawablesSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.meshes = cursor;
        cursor += meshSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.positions = cursor;
        cursor += positionSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.attributes = cursor;
        cursor += attributeSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.indices = cursor;
        cursor += indexSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.materials = cursor;
        cursor += materialSectionSize;
        cursor = alignUp(cursor, Alignment);
        offsets.directionalLights = cursor;
        cursor += directionalLightSectionSize;
        cursor = alignUp(cursor, Alignment);

        for (size_t i = 0; i < loadedSceneData.textureMetadatas.size(); ++i)
        {
            if ((loadedSceneData.textureMetadatas[i].mipTableOffset % sizeof(assets::formats::texture::TextureMipMetadata)) != 0)
            {
                return {vk::Result::eErrorInitializationFailed, {}};
            }
            const uint64_t firstMipIndex =
                loadedSceneData.textureMetadatas[i].mipTableOffset / sizeof(assets::formats::texture::TextureMipMetadata);
            const auto mipCount = static_cast<uint64_t>(loadedSceneData.textureMetadatas[i].mipCount);
            if (firstMipIndex + mipCount > loadedSceneData.textureMipMetadatas.size())
            {
                return {vk::Result::eErrorInitializationFailed, {}};
            }

            auto textureMipDatas =
                std::span(loadedSceneData.textureMipMetadatas.data() +
                              loadedSceneData.textureMetadatas[i].mipTableOffset / sizeof(assets::formats::texture::TextureMipMetadata),
                          loadedSceneData.textureMetadatas[i].mipCount);

            for (size_t j = 0; j < textureMipDatas.size(); ++j)
            {
                const auto& textureMipMetadata = textureMipDatas[j];

                TextureMipUploadInfo textureMipUploadInfo{};
                textureMipUploadInfo.textureIndex = static_cast<uint32_t>(i);
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

        auto [stagingBufferCreateResult, stagingBuffer] = createStagingBuffer(context.allocator, offsets.totalSize);
        if (stagingBufferCreateResult != vk::Result::eSuccess) return {stagingBufferCreateResult, {}};

        void* stagingBufferData = context.allocator.getMappedPointer(*stagingBuffer);

        auto copyToStaging = [stagingBufferData](vk::DeviceSize dstOffset, const void* src, size_t size)
        {
            if (size == 0)
            {
                return;
            }
            std::memcpy(getDestinationAddress(stagingBufferData, dstOffset), src, size);
        };

        SceneGpuInfo sceneGpuInfo {
            .sceneNodesBufferDeviceAddress = context.device.getBufferAddress({.buffer = *nodeBuffer}),
            .materialDatasBufferAddress = context.device.getBufferAddress({.buffer = *materialBuffer}),
            .lightDatasBufferAddress = context.device.getBufferAddress({.buffer = *directionalLightBuffer}),
            .meshDatasBufferAddress = context.device.getBufferAddress({.buffer = *meshBuffer}),
            .drawablesBufferAddress = context.device.getBufferAddress({.buffer = *drawableBuffer}),
            .localTransformsBufferAddress = context.device.getBufferAddress({.buffer = *transformBuffer}),

            .materialCount = static_cast<uint32_t>(loadedSceneData.materials.size()),
            .lightCount = static_cast<uint32_t>(directionalLightGpu.size()),
            .meshCount = static_cast<uint32_t>(loadedSceneData.meshes.size()),
            .drawableCount = static_cast<uint32_t>(loadedSceneData.drawables.size()),
            .nodeCount = static_cast<uint32_t>(loadedSceneData.nodes.size()),
            .padding0 = 0
        };

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
                    context.device.getBufferAddress({.buffer = *positionBuffer}) + meshInfo.positionOffset * sizeof(assets::formats::PositionAttribute),
                .normalUvTangentAttributeBufferAddress =
                    context.device.getBufferAddress({.buffer = *attributeBuffer}) + meshInfo.attributeOffset * sizeof(assets::formats::VertexAttribute),
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
            std::memcpy(getDestinationAddress(stagingBufferData, mip.stagingOffset), loadedSceneData.textureBytes.data() + mip.sourceOffset,
                        mip.dataSize);
        }

        // Запись команд в ресурсы сцены
        if (auto beginCommandBufferResult = cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
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

        std::vector<TextureBarrierInfo> textureBarrierInfos(loadedSceneData.textureMetadatas.size());
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

        if (auto endCommandBufferResult = cmd.end(); endCommandBufferResult != vk::Result::eSuccess)
            return {endCommandBufferResult, {}};

        auto [createFenceResult, fence] = context.device.createFenceUnique({});
        if (createFenceResult != vk::Result::eSuccess) return {createFenceResult, {}};

        vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
            .commandBuffer = cmd,
            .deviceMask = 0x1,
        };

        if (auto submitResult = transferQueue.submit2(
                {{.commandBufferInfoCount = 1, .pCommandBufferInfos = &commandBufferSubmitInfo}}, *fence);
            submitResult != vk::Result::eSuccess)
            return {submitResult, {}};

        if (auto waitFenceResult = context.device.waitForFences({*fence}, vk::True, UINT64_MAX);
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

        return {vk::Result::eSuccess,
                {.hostSceneData = std::move(hostSceneData),
                 .deviceSceneResources = std::move(deviceResources),
                 .sceneFrameRequirements = sceneFrameRequirements}};
    }
} // namespace shuttle::engine::render
