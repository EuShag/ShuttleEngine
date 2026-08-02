//
// Created by Shagu on 27.07.2026.
//
#define GLM_ENABLE_EXPERIMENTAL
#include "Render.hpp"
#include "Assets/Core/BlobReader.hpp"
#include "IncludeVulkan.hpp"
#include "Assets/Formats/Material.hpp"
#include "Assets/Formats/Geometry.hpp"
#include "Assets/Formats/Texture.hpp"
#include <iostream>

#include "../../../external/assimp/include/assimp/TinyFormatter.h"

namespace shuttle::assets::formats::lighting {
    struct DirectionalLight;
}

namespace shuttle::assets::core {
    struct BlobSection;
    class BlobView;
}

namespace shuttle::engine::render
{

struct Texture
{
    resources::UniqueAllocatedImage image;
    vk::UniqueImageView imageView;
};

namespace
{

enum SceneSetBinding : uint32_t
{
    SceneSet_Nodes = 0,
    SceneSet_NodeLevels = 1,
    SceneSet_Transforms = 2,
    SceneSet_Drawables = 3,

    SceneSet_Positions = 4,
    SceneSet_Attributes = 5,
    SceneSet_Meshes = 6,
    SceneSet_Indices = 7,

    SceneSet_Materials = 8,

    SceneSet_DirectionalLights = 9,

    SceneSet_SceneInfo = 10,

    SceneSet_Textures = 11
};

vk::DeviceSize descriptorRange(vk::DeviceSize realSize)
{
    return realSize == 0 ? vk::DeviceSize{16} : realSize;
}

void writeSceneDescriptorSet(vk::Device device, vk::DescriptorSet sceneSet, vk::Buffer nodeBuffer,
                             vk::DeviceSize nodeSectionSize, vk::Buffer levelBuffer, vk::DeviceSize levelSectionSize,
                             vk::Buffer transformBuffer, vk::DeviceSize transformSectionSize, vk::Buffer drawableBuffer,
                             vk::DeviceSize drawableSectionSize, vk::Buffer positionBuffer,
                             vk::DeviceSize positionSectionSize, vk::Buffer attributeBuffer,
                             vk::DeviceSize attributeSectionSize, vk::Buffer meshBuffer, vk::DeviceSize meshSectionSize,
                             vk::Buffer indexBuffer, vk::DeviceSize indexSectionSize, vk::Buffer materialBuffer,
                             vk::DeviceSize materialSectionSize, vk::Buffer directionalLightBuffer,
                             vk::DeviceSize directionalLightSectionSize, vk::Buffer sceneInfoBuffer,
                             vk::DeviceSize sceneInfoBufferSize, const TextureCatalog& textureCatalog)
{
    std::array bufferInfos{
        vk::DescriptorBufferInfo{.buffer = nodeBuffer, .offset = 0, .range = descriptorRange(nodeSectionSize)},

        vk::DescriptorBufferInfo{.buffer = levelBuffer, .offset = 0, .range = descriptorRange(levelSectionSize)},

        vk::DescriptorBufferInfo{
            .buffer = transformBuffer, .offset = 0, .range = descriptorRange(transformSectionSize)},

        vk::DescriptorBufferInfo{.buffer = drawableBuffer, .offset = 0, .range = descriptorRange(drawableSectionSize)},

        vk::DescriptorBufferInfo{.buffer = positionBuffer, .offset = 0, .range = descriptorRange(positionSectionSize)},

        vk::DescriptorBufferInfo{
            .buffer = attributeBuffer, .offset = 0, .range = descriptorRange(attributeSectionSize)},

        vk::DescriptorBufferInfo{.buffer = meshBuffer, .offset = 0, .range = descriptorRange(meshSectionSize)},

        vk::DescriptorBufferInfo{.buffer = indexBuffer, .offset = 0, .range = descriptorRange(indexSectionSize)},

        vk::DescriptorBufferInfo{.buffer = materialBuffer, .offset = 0, .range = descriptorRange(materialSectionSize)},

        vk::DescriptorBufferInfo{
            .buffer = directionalLightBuffer, .offset = 0, .range = descriptorRange(directionalLightSectionSize)},

        vk::DescriptorBufferInfo{
            .buffer = sceneInfoBuffer, .offset = 0, .range = descriptorRange(sceneInfoBufferSize)}};

    std::array const writes{vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Nodes,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[0]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_NodeLevels,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[1]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Transforms,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[2]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Drawables,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[3]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Positions,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[4]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Attributes,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[5]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Meshes,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[6]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Indices,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[7]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_Materials,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[8]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_DirectionalLights,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                   .pBufferInfo = &bufferInfos[9]},

                            vk::WriteDescriptorSet{.dstSet = sceneSet,
                                                   .dstBinding = SceneSet_SceneInfo,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                   .pBufferInfo = &bufferInfos[10]}};

    device.updateDescriptorSets(writes, {});

    textureCatalog.writeDescriptors(device, sceneSet, SceneSet_Textures);
}

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

void writeBufferAndImageDestinationBarrier(vk::CommandBuffer commandBuffer,
                                           std::vector<TextureBarrierInfo> const& textures,
                                           std::vector<BufferBarrierInfo> const& buffers)
{

    std::vector<vk::ImageMemoryBarrier2> textureBarriers(textures.size());
    std::vector<vk::BufferMemoryBarrier2> bufferBarriers(buffers.size());

    for (uint32_t i = 0; i < textureBarriers.size(); ++i)
    {
        textureBarriers[i] = {.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                              .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                              .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                              .dstAccessMask = vk::AccessFlagBits2::eNone,
                              .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                              .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                              .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                              .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                              .image = textures[i].image,
                              .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
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

template <typename T>
vk::ResultValue<std::span<const T>> readTypedSection(const shuttle::assets::core::BlobView& blob,
                                                     shuttle::assets::core::BlobSectionType type)
{

    const std::optional<shuttle::assets::core::BlobSection> section = blob.findSection(type);
    if (!section) return {vk::Result::eSuccess, {}};

    const std::span<const uint8_t> bytes = blob.bytes(*section);

    if (bytes.empty()) return {vk::Result::eSuccess, {}};

    if (bytes.size_bytes() % sizeof(T) != 0)
    {
        return {vk::Result::eErrorOutOfHostMemory, {}};
    }

    return {vk::Result::eSuccess,
            std::span<const T>(reinterpret_cast<const T*>(bytes.data()), bytes.size_bytes() / sizeof(T))};
}

uint32_t countShadowCastersDirectional(std::span<const assets::formats::lighting::DirectionalLight> lights)
{
    uint32_t count = 0;
    for (const auto& light : lights)
        if (light.castShadows != 0) ++count;
    return count;
}

vk::ResultValue<std::span<const uint8_t>> readRawSection(const shuttle::assets::core::BlobView& blob,
                                                         shuttle::assets::core::BlobSectionType type)
{

    const std::optional<shuttle::assets::core::BlobSection> section = blob.findSection(type);
    if (!section) return {vk::Result::eErrorOutOfHostMemory, {}};

    return {vk::Result::eSuccess, blob.bytes(*section)};
}

vk::ResultValue<resources::UniqueAllocatedBuffer> createDeviceStorageBuffer(resources::DeviceAllocator& allocator,
                                                                            vk::DeviceSize size)
{
    return allocator.createAndAllocateBufferUnique(
        vk::BufferCreateInfo{.size = size == 0 ? 16 : size,
                             .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                             .sharingMode = vk::SharingMode::eExclusive},
        resources::MemoryUsage::eGpuOnly);
}

vk::ResultValue<resources::UniqueAllocatedBuffer> createDeviceUniformBuffer(resources::DeviceAllocator& allocator,
                                                                            vk::DeviceSize size)
{
    return allocator.createAndAllocateBufferUnique(
        vk::BufferCreateInfo{.size = size == 0 ? 16 : size,
                             .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                             .sharingMode = vk::SharingMode::eExclusive},
        resources::MemoryUsage::eGpuOnly);
}

vk::ResultValue<Texture> createTexture(vk::Device device, resources::DeviceAllocator& allocator, vk::Extent2D size,
                                       vk::Format format, uint32_t mipLevels)
{

    auto [createImageResult, textureImage] = allocator.createAndAllocateImageUnique(
        vk::ImageCreateInfo{.imageType = vk::ImageType::e2D,
                            .format = format,
                            .extent = vk::Extent3D{.width = size.width, .height = size.height, .depth = 1},
                            .mipLevels = mipLevels,
                            .arrayLayers = 1,
                            .samples = vk::SampleCountFlagBits::e1,
                            .tiling = vk::ImageTiling::eOptimal,
                            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                            .initialLayout = vk::ImageLayout::eUndefined},
        resources::MemoryUsage::eGpuOnly);
    if (createImageResult != vk::Result::eSuccess) return {createImageResult, {}};

    auto [createImageViewResult, textureImageView] = device.createImageViewUnique(vk::ImageViewCreateInfo{
        .image = *textureImage,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                      .baseMipLevel = 0,
                                                      .levelCount = mipLevels,
                                                      .baseArrayLayer = 0,
                                                      .layerCount = 1}});
    if (createImageViewResult != vk::Result::eSuccess) return {createImageViewResult, {}};
    return {vk::Result::eSuccess, Texture{std::move(textureImage), std::move(textureImageView)}};
}

vk::ResultValue<resources::UniqueAllocatedBuffer> createStagingBuffer(resources::DeviceAllocator& allocator,
                                                                      vk::DeviceSize size)
{
    return allocator.createAndAllocateBufferUnique(
        vk::BufferCreateInfo{.size = size,
                             .usage = vk::BufferUsageFlagBits::eTransferSrc,
                             .sharingMode = vk::SharingMode::eExclusive},
        resources::MemoryUsage::eCpuToGpu, resources::AllocationCreateFlagBits::eMapped);
}

vk::ResultValue<resources::UniqueAllocatedBuffer> createIndexBuffer(resources::DeviceAllocator& allocator,
                                                                    vk::DeviceSize size)
{
    return allocator.createAndAllocateBufferUnique(
        vk::BufferCreateInfo{.size = size == 0 ? 16 : size,
                             .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                      vk::BufferUsageFlagBits::eTransferDst,
                             .sharingMode = vk::SharingMode::eExclusive},
        resources::MemoryUsage::eGpuOnly);
}
} // namespace

vk::ResultValue<UploadSceneOutput>
uploadScene(const std::filesystem::path& scenePath, RenderContext& context, vk::Queue transferQueue,
            vk::CommandPool transferCommandPool, vk::DescriptorSetLayout sceneSetLayout,
            vk::ImageView fallbackAlbedoImageView, vk::ImageView fallbackNormalImageView,
            vk::ImageView fallbackOrmImageView, vk::ImageView fallbackEmissionImageView)
{

    std::vector<resources::UniqueAllocatedBuffer> stagingBuffers;

    assets::core::BlobView blob = assets::core::BlobReader::open(scenePath);

    auto [allocateCommandBufferResult, cmdUnique] = allocateOneTimeCommandBuffer(context.device, transferCommandPool);
    if (allocateCommandBufferResult != vk::Result::eSuccess) return {allocateCommandBufferResult, {}};
    vk::CommandBuffer cmd = *cmdUnique;

    const auto [readNodesResult, nodes] =
        readTypedSection<assets::formats::scene::SceneNode>(blob, assets::core::BlobSectionType::GpuSceneNodes);
    const auto [readLevelsResult, levels] =
        readTypedSection<assets::formats::scene::NodeLevelRange>(blob, assets::core::BlobSectionType::GpuNodeLevels);
    const auto [readTransformsResult, transforms] =
        readTypedSection<assets::formats::scene::Transform>(blob, assets::core::BlobSectionType::GpuSceneTransforms);
    const auto [readDrawablesResult, drawables] = readTypedSection<assets::formats::scene::GpuDrawableObject>(
        blob, assets::core::BlobSectionType::GpuDrawableObjects);
    const auto [readDirectionalLightsResult, directionalLights] =
        readTypedSection<assets::formats::lighting::DirectionalLight>(
            blob, assets::core::BlobSectionType::GpuDirectionalLights);
    const auto [readPositionsResult, positions] =
        readTypedSection<assets::formats::PositionAttribute>(blob, assets::core::BlobSectionType::PositionMegabuffer);
    const auto [readAttributesResult, attributes] =
        readTypedSection<assets::formats::VertexAttribute>(blob, assets::core::BlobSectionType::AttributeMegabuffer);
    const auto [readIndicesResult, indices] =
        readTypedSection<uint32_t>(blob, assets::core::BlobSectionType::IndexMegabuffer);
    const auto [readMeshesResult, meshes] =
        readTypedSection<assets::formats::geometry::GpuMesh>(blob, assets::core::BlobSectionType::GpuMeshes);
    const auto [readMaterialsResult, materials] =
        readTypedSection<assets::formats::material::MaterialInfo>(blob, assets::core::BlobSectionType::GpuMaterials);
    const auto [readTextureMetadataResult, textureMetadatas] =
        readTypedSection<assets::formats::texture::TextureMetadata>(blob,
                                                                    assets::core::BlobSectionType::TextureMetadata);
    const auto [readTextureMipMetadataResult, textureMipMetadatas] =
        readTypedSection<assets::formats::texture::TextureMipMetadata>(
            blob, assets::core::BlobSectionType::TextureMipMetadata);
    const auto [readTextureBytesResult, textureBytes] =
        readRawSection(blob, assets::core::BlobSectionType::TextureData);

    if (readNodesResult != vk::Result::eSuccess) return {readNodesResult, {}};
    if (readLevelsResult != vk::Result::eSuccess) return {readLevelsResult, {}};
    if (readTransformsResult != vk::Result::eSuccess) return {readTransformsResult, {}};
    if (readDrawablesResult != vk::Result::eSuccess) return {readDrawablesResult, {}};
    if (readDirectionalLightsResult != vk::Result::eSuccess) return {readDirectionalLightsResult, {}};
    if (readPositionsResult != vk::Result::eSuccess) return {readPositionsResult, {}};
    if (readAttributesResult != vk::Result::eSuccess) return {readAttributesResult, {}};
    if (readIndicesResult != vk::Result::eSuccess) return {readIndicesResult, {}};
    if (readMeshesResult != vk::Result::eSuccess) return {readMeshesResult, {}};
    if (readMaterialsResult != vk::Result::eSuccess) return {readMaterialsResult, {}};
    if (readTextureMetadataResult != vk::Result::eSuccess) return {readTextureMetadataResult, {}};
    if (readTextureMipMetadataResult != vk::Result::eSuccess) return {readTextureMipMetadataResult, {}};
    if (readTextureBytesResult != vk::Result::eSuccess) return {readTextureBytesResult, {}};

    const size_t nodeSectionSize = nodes.size_bytes();
    const size_t levelSectionSize = levels.size_bytes();
    const size_t transformSectionSize = transforms.size_bytes();
    std::vector<assets::formats::material::MaterialInfo> materialData(materials.begin(), materials.end());
    if (materialData.empty())
    {
        materialData.emplace_back();
    }

    std::vector<assets::formats::scene::GpuDrawableObject> drawableData(drawables.begin(), drawables.end());
    if (meshes.empty() || transforms.empty())
    {
        drawableData.clear();
    }
    else
    {
        for (auto& drawable : drawableData)
        {
            if (drawable.transformIndex >= transforms.size())
            {
                drawable.transformIndex = 0;
            }
            if (drawable.meshIndex >= meshes.size())
            {
                drawable.meshIndex = 0;
                drawable.flags = 0;
            }
            if (drawable.materialIndex >= materialData.size())
            {
                drawable.materialIndex = 0;
            }
        }
    }

    std::vector<uint32_t> indexData(indices.begin(), indices.end());
    bool convertedLegacyGlobalIndices = false;
    for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
    {
        const auto& mesh = meshes[meshIndex];
        if (mesh.lodCount > assets::formats::geometry::MaxMeshLods)
        {
            return {vk::Result::eErrorInitializationFailed, {}};
        }
        if (mesh.positionOffset == 0 || mesh.lodCount == 0)
        {
            continue;
        }

        bool hasGlobalIndices = false;
        for (uint32_t lodIndex = 0; lodIndex < mesh.lodCount; ++lodIndex)
        {
            const auto& lod = mesh.lods[lodIndex];
            if (lod.indexCount == 0)
            {
                continue;
            }
            const uint64_t endIndex = static_cast<uint64_t>(lod.firstIndex) + lod.indexCount;
            if (endIndex > indexData.size())
            {
                return {vk::Result::eErrorInitializationFailed, {}};
            }
            bool allIndicesUseGlobalBase = true;
            for (uint32_t i = 0; i < lod.indexCount; ++i)
            {
                const uint32_t idx = indexData[lod.firstIndex + i];
                if (idx < mesh.positionOffset)
                {
                    allIndicesUseGlobalBase = false;
                    break;
                }
            }
            if (allIndicesUseGlobalBase)
            {
                hasGlobalIndices = true;
                break;
            }
        }

        if (!hasGlobalIndices)
        {
            continue;
        }

        for (uint32_t lodIndex = 0; lodIndex < mesh.lodCount; ++lodIndex)
        {
            const auto& lod = mesh.lods[lodIndex];
            if (lod.indexCount == 0)
            {
                continue;
            }
            for (uint32_t i = 0; i < lod.indexCount; ++i)
            {
                uint32_t& idx = indexData[lod.firstIndex + i];
                if (idx < mesh.positionOffset)
                {
                    return {vk::Result::eErrorInitializationFailed, {}};
                }
                idx -= mesh.positionOffset;
            }
        }
        convertedLegacyGlobalIndices = true;
    }
    if (convertedLegacyGlobalIndices)
    {
        std::cerr << "[Scene] Converted legacy global mesh indices to local indices during upload.\n";
    }

    const size_t drawableSectionSize = drawableData.size() * sizeof(assets::formats::scene::GpuDrawableObject);
    const size_t positionSectionSize = positions.size_bytes();
    const size_t attributeSectionSize = attributes.size_bytes();
    const size_t indexSectionSize = indexData.size() * sizeof(uint32_t);
    const size_t meshSectionSize = meshes.size_bytes();
    const size_t materialSectionSize = materialData.size() * sizeof(assets::formats::material::MaterialInfo);
    const size_t directionalLightSectionSize = directionalLights.size_bytes();

    HostSceneData hostSceneData{};
    hostSceneData.nodes.assign(nodes.begin(), nodes.end());
    hostSceneData.levels.assign(levels.begin(), levels.end());
    hostSceneData.transforms.assign(transforms.begin(), transforms.end());
    hostSceneData.drawableObjects = drawableData;
    hostSceneData.directionalLights.assign(directionalLights.begin(), directionalLights.end());

    SceneFrameRequirements sceneFrameRequirements{};
    sceneFrameRequirements.transformCount = static_cast<uint32_t>(transforms.size());
    sceneFrameRequirements.drawableObjectCount = static_cast<uint32_t>(drawableData.size());
    sceneFrameRequirements.directionalShadowCasterCount = countShadowCastersDirectional(directionalLights);
    if (sceneFrameRequirements.directionalShadowCasterCount == 0)
        sceneFrameRequirements.directionalShadowCasterCount = 1;
    sceneFrameRequirements.pointShadowCasterCount = 0;
    sceneFrameRequirements.spotShadowCasterCount = 0;
    sceneFrameRequirements.meshCount = static_cast<uint32_t>(meshes.size());

    for (uint32_t i = 0; i < materials.size(); ++i) {
        printf(
    "Material %u: albedo=%u normal=%u orm=%u\n",
        i,
        materials[i].albedoTexture,
        materials[i].normalTexture,
        materials[i].ormTexture);
    }

    assets::formats::lighting::DirectionalLight fallbackDirectionalLightData{.directionAndIntensity =
                                                                                 glm::vec4{0.0f, -1.0f, 0.0f, 100.0f},
                                                                             .color = glm::vec3{1.0f, 1.0f, 1.0f},
                                                                             .castShadows = 1};

    SceneInfo sceneInfo{    .drawableObjectCount = static_cast<uint32_t>(drawableData.size()),
                        .transformCount = static_cast<uint32_t>(transforms.size()),
                        .directionalLightCount = static_cast<uint32_t>(directionalLights.size()) == 0
                                                     ? 1
                                                     : static_cast<uint32_t>(directionalLights.size()),
                        .directionalShadowCasterCount = sceneFrameRequirements.directionalShadowCasterCount,
                        .materialCount = static_cast<uint32_t>(materialData.size()),
        .textureCount = assets::formats::texture::TextureIndices::FirstUserTexture + static_cast<int32_t>(textureMetadatas.size())};

    TextureCatalog textureCatalog{fallbackAlbedoImageView, fallbackNormalImageView, fallbackOrmImageView,
                                  fallbackEmissionImageView, static_cast<uint32_t>(textureMetadatas.size())};

    std::vector<resources::UniqueAllocatedImage> textureImages;
    textureImages.reserve(textureMetadatas.size());

    std::vector<vk::UniqueImageView> textureImageViews;
    textureImageViews.reserve(textureMetadatas.size());

    for (const auto& textureMetadata : textureMetadatas)
    {
        if (textureMetadata.mipCount == 0 || textureMetadata.width == 0 || textureMetadata.height == 0 ||
            textureMetadata.depth != 1 || textureMetadata.layerCount != 1 ||
            textureMetadata.imageType != assets::formats::texture::ImageType::Image2D ||
            textureMetadata.imageViewType != assets::formats::texture::ImageViewType::View2D)
        {
            return {vk::Result::eErrorInitializationFailed, {}};
        }

        auto [createImageResult, texture] =
            createTexture(context.device, context.allocator,
                          vk::Extent2D{.width = textureMetadata.width, .height = textureMetadata.height},
                          static_cast<vk::Format>(textureMetadata.format), textureMetadata.mipCount);
        if (createImageResult != vk::Result::eSuccess) return {createImageResult, {}};
        textureImages.push_back(std::move(texture.image));
        textureImageViews.push_back(std::move(texture.imageView));
        textureCatalog.addTextureView(*textureImageViews.back());
    }

    auto [createSceneInfoBufferResult, sceneInfoBuffer] =
        createDeviceUniformBuffer(context.allocator, sizeof(SceneInfo));
    auto [createMeshBufferResult, meshBuffer] = createDeviceStorageBuffer(context.allocator, meshSectionSize);
    auto [createPositionBufferResult, positionBuffer] =
        createDeviceStorageBuffer(context.allocator, positionSectionSize);
    auto [createAttributeBufferResult, attributeBuffer] =
        createDeviceStorageBuffer(context.allocator, attributeSectionSize);
    auto [createIndexBufferResult, indexBuffer] = createIndexBuffer(context.allocator, indexSectionSize);
    auto [createMaterialBufferResult, materialBuffer] =
        createDeviceStorageBuffer(context.allocator, materialSectionSize);
    auto [createNodeBufferResult, nodeBuffer] = createDeviceStorageBuffer(context.allocator, nodeSectionSize);
    auto [createLevelBufferResult, levelBuffer] = createDeviceStorageBuffer(context.allocator, levelSectionSize);
    auto [createTransformBufferResult, transformBuffer] =
        createDeviceStorageBuffer(context.allocator, transformSectionSize);
    auto [createDrawableBufferResult, drawableBuffer] =
        createDeviceStorageBuffer(context.allocator, drawableSectionSize);
    auto [createDirectionalLightBufferResult, directionalLightBuffer] = createDeviceStorageBuffer(
        context.allocator, directionalLightSectionSize == 0 ? sizeof(assets::formats::lighting::DirectionalLight)
                                                            : directionalLightSectionSize);

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
    cursor += sizeof(SceneInfo);
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
    cursor += drawableSectionSize;
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
    cursor += directionalLightSectionSize == 0 ? sizeof(assets::formats::lighting::DirectionalLight)
                                               : directionalLightSectionSize;
    cursor = alignUp(cursor, Alignment);

    for (size_t i = 0; i < textureMetadatas.size(); ++i)
    {
        if ((textureMetadatas[i].mipTableOffset % sizeof(assets::formats::texture::TextureMipMetadata)) != 0)
        {
            return {vk::Result::eErrorInitializationFailed, {}};
        }
        const uint64_t firstMipIndex =
            textureMetadatas[i].mipTableOffset / sizeof(assets::formats::texture::TextureMipMetadata);
        const uint64_t mipCount = static_cast<uint64_t>(textureMetadatas[i].mipCount);
        if (firstMipIndex + mipCount > textureMipMetadatas.size())
        {
            return {vk::Result::eErrorInitializationFailed, {}};
        }

        auto textureMipDatas =
            std::span(textureMipMetadatas.data() +
                          textureMetadatas[i].mipTableOffset / sizeof(assets::formats::texture::TextureMipMetadata),
                      textureMetadatas[i].mipCount);

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
            if (textureMipUploadInfo.sourceOffset + textureMipUploadInfo.dataSize > textureBytes.size())
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

    copyToStaging(offsets.sceneInfo, &sceneInfo, sizeof(SceneInfo));
    copyToStaging(offsets.nodes, nodes.data(), nodeSectionSize);
    copyToStaging(offsets.levels, levels.data(), levelSectionSize);
    copyToStaging(offsets.transforms, transforms.data(), transformSectionSize);
    copyToStaging(offsets.drawables, drawableData.data(), drawableSectionSize);
    copyToStaging(offsets.meshes, meshes.data(), meshSectionSize);
    copyToStaging(offsets.positions, positions.data(), positionSectionSize);
    copyToStaging(offsets.attributes, attributes.data(), attributeSectionSize);
    copyToStaging(offsets.indices, indexData.data(), indexSectionSize);
    copyToStaging(offsets.materials, materialData.data(), materialSectionSize);
    copyToStaging(offsets.directionalLights, directionalLightSectionSize == 0 ? &fallbackDirectionalLightData : directionalLights.data(),
                  directionalLightSectionSize == 0 ? sizeof(assets::formats::lighting::DirectionalLight)
                                                   : directionalLightSectionSize);

    for (const auto& mip : offsets.textureMipUploadInfos)
    {
        std::memcpy(getDestinationAddress(stagingBufferData, mip.stagingOffset), textureBytes.data() + mip.sourceOffset,
                    mip.dataSize);
    }

    // Запись команд в ресурсы сцены
    if (auto beginCommandBufferResult = cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        beginCommandBufferResult != vk::Result::eSuccess)
        return {beginCommandBufferResult, {}};

    std::vector<CopyBufferCommandInfo> copyBufferInfos{
        {.stagingOffset = offsets.sceneInfo, .destinationBuffer = *sceneInfoBuffer, .dataSize = sizeof(SceneInfo)},
        {.stagingOffset = offsets.nodes, .destinationBuffer = *nodeBuffer, .dataSize = nodeSectionSize},
        {.stagingOffset = offsets.levels, .destinationBuffer = *levelBuffer, .dataSize = levelSectionSize},
        {.stagingOffset = offsets.transforms, .destinationBuffer = *transformBuffer, .dataSize = transformSectionSize},
        {.stagingOffset = offsets.drawables, .destinationBuffer = *drawableBuffer, .dataSize = drawableSectionSize},
        {.stagingOffset = offsets.meshes, .destinationBuffer = *meshBuffer, .dataSize = meshSectionSize},
        {.stagingOffset = offsets.positions, .destinationBuffer = *positionBuffer, .dataSize = positionSectionSize},
        {.stagingOffset = offsets.attributes, .destinationBuffer = *attributeBuffer, .dataSize = attributeSectionSize},
        {.stagingOffset = offsets.indices, .destinationBuffer = *indexBuffer, .dataSize = indexSectionSize},
        {.stagingOffset = offsets.materials, .destinationBuffer = *materialBuffer, .dataSize = materialSectionSize},
        {.stagingOffset = offsets.directionalLights,
         .destinationBuffer = *directionalLightBuffer,
         .dataSize = directionalLightSectionSize == 0 ? sizeof(assets::formats::lighting::DirectionalLight)
                                                      : directionalLightSectionSize},
    };

    std::vector<TextureBarrierInfo> textureBarrierInfos(textureMetadatas.size());
    for (size_t i = 0; i < textureMetadatas.size(); ++i)
    {
        textureBarrierInfos[i] = {.image = *textureImages[i], .mipLevels = textureMetadatas[i].mipCount};
    }
    std::vector<BufferBarrierInfo> bufferBarrierInfos{
        {.buffer = *nodeBuffer, .size = nodeSectionSize},
        {.buffer = *levelBuffer, .size = levelSectionSize},
        {.buffer = *transformBuffer, .size = transformSectionSize},
        {.buffer = *drawableBuffer, .size = drawableSectionSize},
        {.buffer = *meshBuffer, .size = meshSectionSize},
        {.buffer = *positionBuffer, .size = positionSectionSize},
        {.buffer = *attributeBuffer, .size = attributeSectionSize},
        {.buffer = *indexBuffer, .size = indexSectionSize},
        {.buffer = *materialBuffer, .size = materialSectionSize},
        {.buffer = *directionalLightBuffer,
         .size = directionalLightSectionSize == 0 ? sizeof(assets::formats::lighting::DirectionalLight)
                                                  : directionalLightSectionSize},
    };

    std::vector<vk::Image> images(textureImages.size());
    for (size_t i = 0; i < textureImages.size(); ++i)
    {
        images[i] = *textureImages[i];
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

    auto [createSceneSetResult, sceneSet] = context.device.allocateDescriptorSets(
        {.descriptorPool = *context.descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &sceneSetLayout});
    if (createSceneSetResult != vk::Result::eSuccess) return {createSceneSetResult, {}};

    writeSceneDescriptorSet(context.device, sceneSet[0], *nodeBuffer, nodeSectionSize, *levelBuffer, levelSectionSize,
                            *transformBuffer, transformSectionSize, *drawableBuffer, drawableSectionSize,
                            *positionBuffer, positionSectionSize, *attributeBuffer, attributeSectionSize, *meshBuffer,
                            meshSectionSize, *indexBuffer, indexSectionSize, *materialBuffer, materialSectionSize,
                            *directionalLightBuffer, directionalLightSectionSize == 0 ? sizeof(assets::formats::lighting::DirectionalLight) : directionalLightSectionSize, *sceneInfoBuffer, sizeof(SceneInfo),
                            textureCatalog);

    DeviceSceneResources deviceResources{.sceneInfoBuffer = std::move(sceneInfoBuffer),
                                         .positionBuffer = std::move(positionBuffer),
                                         .attributeBuffer = std::move(attributeBuffer),
                                         .indexBuffer = std::move(indexBuffer),
                                         .meshBuffer = std::move(meshBuffer),

                                         .directionalLightsBuffer = std::move(directionalLightBuffer),

                                         .materialSsbo = std::move(materialBuffer),

                                         .textureImages = std::move(textureImages),
                                         .textureImageViews = std::move(textureImageViews),
                                         .textureCatalog = std::move(textureCatalog),

                                         .sceneNodeBuffer = std::move(nodeBuffer),
                                         .sceneLevelBuffer = std::move(levelBuffer),
                                         .gpuDrawableObjectsBuffer = std::move(drawableBuffer),
                                         .sceneTransformBuffer = std::move(transformBuffer),

                                         .sceneSet = sceneSet[0]};

    return {vk::Result::eSuccess,
            {.hostSceneData = std::move(hostSceneData),
             .deviceSceneResources = std::move(deviceResources),
             .sceneFrameRequirements = sceneFrameRequirements}};
}
} // namespace shuttle::engine::render
