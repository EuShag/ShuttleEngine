//
// Created by Shagu on 31.07.2026.
//
#include "Render.hpp"
#include <Assets/Core/BlobView.hpp>
#include <Assets/Core/BlobFormat.hpp>
#include <Assets/Formats/Environment.hpp>
#include <Assets/Formats/Texture.hpp>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>
namespace shuttle::engine::render
{
namespace
{
// ============================================================
// Image type conversion
// ============================================================
vk::ImageType toVkImageType(assets::formats::texture::ImageType type)
{
    using T = assets::formats::texture::ImageType;
    switch (type)
    {
        case T::Image1D: return vk::ImageType::e1D;
        case T::Image2D: return vk::ImageType::e2D;
        case T::Image3D: return vk::ImageType::e3D;
        default: return vk::ImageType::e2D;
    }
}
vk::ImageViewType toVkImageViewType(assets::formats::texture::ImageViewType type)
{
    using T = assets::formats::texture::ImageViewType;
    switch (type)
    {
        case T::View1D: return vk::ImageViewType::e1D;
        case T::View2D: return vk::ImageViewType::e2D;
        case T::View3D: return vk::ImageViewType::e3D;
        case T::ViewCube: return vk::ImageViewType::eCube;
        case T::View1DArray: return vk::ImageViewType::e1DArray;
        case T::View2DArray: return vk::ImageViewType::e2DArray;
        case T::ViewCubeArray: return vk::ImageViewType::eCubeArray;
        default: return vk::ImageViewType::e2D;
    }
}
bool isCubeViewType(assets::formats::texture::ImageViewType type)
{
    using T = assets::formats::texture::ImageViewType;
    return type == T::ViewCube || type == T::ViewCubeArray;
}
// ============================================================
// Read helpers
// ============================================================
template <typename T>
std::span<const T> readSectionSpan(const assets::core::BlobView& blob, assets::core::BlobSectionType sectionType)
{
    auto section = blob.findSection(sectionType);
    if (!section)
    {
        return {};
    }
    auto bytes = blob.bytes(*section);
    if (bytes.empty())
    {
        return {};
    }
    return std::span<const T>(reinterpret_cast<const T*>(bytes.data()), bytes.size() / sizeof(T));
}
std::span<const uint8_t> readSectionBytes(const assets::core::BlobView& blob, assets::core::BlobSectionType sectionType)
{
    auto section = blob.findSection(sectionType);
    if (!section)
    {
        return {};
    }
    return blob.bytes(*section);
}
// ===================================*=======================
// Upload one environment texture
// ===========================*================================
vk::Result uploadEnvironmentTexture(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
resources::DeviceAllocator const& allocator,
const assets::formats::texture::TextureMetadata& metadata,
std::span<const assets::formats::texture::TextureMipMetadata> mipMetadata,
std::span<const uint8_t> textureData,
resources::UniqueAllocatedImage& outImage, vk::UniqueImageView& outImageView)
{
    const auto format = static_cast<vk::Format>(metadata.format);
    const auto imageType = toVkImageType(metadata.imageType);
    const auto viewType = toVkImageViewType(metadata.imageViewType);
    const uint32_t mipCount = metadata.mipCount;
    const uint32_t layerCount = metadata.layerCount;
    vk::ImageCreateFlags imageFlags{};
    if (isCubeViewType(metadata.imageViewType))
    {
        imageFlags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }
    // ==================================*=====================
    // Build upload buffer and copy regions
    // ===============*==================================*=====
    std::vector<std::byte> uploadBytes;
    std::vector<vk::BufferImageCopy> copyRegions;
    copyRegions.reserve(mipCount);
    vk::DeviceSize bufferOffset = 0;
    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        if (mip >= mipMetadata.size())
        {
            return vk::Result::eErrorInitializationFailed;
        }
        const auto& mipInfo = mipMetadata[mip];
        if (mipInfo.dataOffset + mipInfo.dataSize > textureData.size())
        {
            return vk::Result::eErrorInitializationFailed;
        }
        const uint8_t* src = textureData.data() + mipInfo.dataOffset;
        const auto dataSize = static_cast<size_t>(mipInfo.dataSize);
        const size_t oldSize = uploadBytes.size();
        uploadBytes.resize(oldSize + dataSize);
        std::memcpy(uploadBytes.data() + oldSize, src, dataSize);
        copyRegions.push_back(vk::BufferImageCopy{
            .bufferOffset = bufferOffset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = vk::ImageSubresourceLayers{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                           .mipLevel = mip,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = layerCount},
            .imageOffset = vk::Offset3D{0, 0, 0},
            .imageExtent = vk::Extent3D{mipInfo.width, mipInfo.height, 1}});
        bufferOffset += mipInfo.dataSize;
    }
    if (uploadBytes.empty() || copyRegions.empty())
    {
        return vk::Result::eErrorInitializationFailed;
    }
    // ========================================================
    // Staging buffer
    // ========================================================
    auto [stagingResult, stagingBuffer] =
    allocator.createAndAllocateBufferUnique(vk::BufferCreateInfo{.size = uploadBytes.size(),
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive},
    resources::MemoryUsage::eCpuOnly);
    if (stagingResult != vk::Result::eSuccess)
    {
        return stagingResult;
    }
    if (auto result = allocator.writeBufferFromHost({.dstBuffer = *stagingBuffer,
        .dstBufferOffset = 0,
        .srcData = uploadBytes.data(),
        .dataSize = uploadBytes.size()});
    result != vk::Result::eSuccess)
    {
        return result;
    }
    // ==============*==================================*======
    // GPU image
    // // ======================*=================================
    auto [imageResult, image] = allocator.createAndAllocateImageUnique(
    vk::ImageCreateInfo{.flags = imageFlags,
        .imageType = imageType,
        .format = format,
        .extent = vk::Extent3D{metadata.width, metadata.height, metadata.depth},
        .mipLevels = mipCount,
        .arrayLayers = layerCount,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined},
    resources::MemoryUsage::eGpuOnly);
    if (imageResult != vk::Result::eSuccess)
    {
        return imageResult;
    }
    // ========================================================
    // Image view
    // ========================================================
    auto [viewResult, imageView] = device.createImageViewUnique(vk::ImageViewCreateInfo{
        .image = *image,
        .viewType = viewType,
        .format = format,
        .subresourceRange = vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = mipCount,
            .baseArrayLayer = 0,
            .layerCount = layerCount}});
    if (viewResult != vk::Result::eSuccess)
    {
        return viewResult;
    }
    // ========================================================
    // Command buffer
    // ========================================================
    auto [cmdResult, commandBuffers] =
    device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{.commandPool = transferCommandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1});
    if (cmdResult != vk::Result::eSuccess)
    {
        return cmdResult;
    }
    vk::CommandBuffer cmd = commandBuffers[0].get();
    if (auto result = cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    result != vk::Result::eSuccess)
    {
        return result;
    }
    auto transitionImage = [&cmd, &image, mipCount, layerCount](vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
    vk::PipelineStageFlags srcStage,
    vk::PipelineStageFlags dstStage)
    {
        vk::ImageMemoryBarrier barrier{.srcAccessMask = srcAccess,
            .dstAccessMask = dstAccess,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = *image,
            .subresourceRange =
            vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = mipCount,
                .baseArrayLayer = 0,
                .layerCount = layerCount}};
        cmd.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
    };
    transitionImage(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
    vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTopOfPipe,
    vk::PipelineStageFlagBits::eTransfer);
    cmd.copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, copyRegions);
    transitionImage(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::AccessFlagBits::eTransferWrite, {},
    vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe);
    if (auto result = cmd.end(); result != vk::Result::eSuccess)
    {
        return result;
    }
    vk::SubmitInfo submitInfo{.commandBufferCount = 1,
        .pCommandBuffers = &cmd};
    if (auto result = transferQueue.submit(1, &submitInfo, nullptr); result != vk::Result::eSuccess)
    {
        return result;
    }
    if (auto result = transferQueue.waitIdle(); result != vk::Result::eSuccess)
    {
        return result;
    }
    outImage = std::move(image);
    outImageView = std::move(imageView);
    return vk::Result::eSuccess;
}
} // namespace
// ============================================================
// createEnvironmentResources
// ============================================================
vk::ResultValue<DeviceEnvironmentResources> createEnvironmentResources(RenderContext& context,
const DeviceRendererResources& rendererResources,
vk::Queue transferQueue,
vk::CommandPool transferCommandPool,
const std::filesystem::path& environmentBlobPath)
{
    using namespace shuttle::assets::core;
    using namespace shuttle::assets::formats::environment;
    using namespace shuttle::assets::formats::texture;
    DeviceEnvironmentResources resources{};
    std::cout << "[Environment] Loading environment blob: " << environmentBlobPath.string() << std::endl;
    // ========================================================
    // Open blob
    // ========================================================
    BlobView blob = BlobView::open(environmentBlobPath);
    // ========================================================
    // Read sections
    // ========================================================
    auto environmentInfos = readSectionSpan<EnvironmentInfo>(blob, BlobSectionType::EnvironmentInfo);
    const EnvironmentInfo& environmentInfo = environmentInfos[0];
    auto textureMetadatas = readSectionSpan<TextureMetadata>(blob, BlobSectionType::EnvironmentTextureMetadata);
    auto allMipMetadata = readSectionSpan<TextureMipMetadata>(blob, BlobSectionType::EnvironmentTextureMipMetadata);
    auto textureData = readSectionBytes(blob, BlobSectionType::EnvironmentTextureData);
    if (environmentInfos.empty() || textureMetadatas.empty() || allMipMetadata.empty() || textureData.empty())
    {
        std::cerr << "[Environment] Missing environment sections." << std::endl;
        return {vk::Result::eErrorInitializationFailed, {}};
    }
    // ========================================================
    // Local texture loader
    // ========================================================
    auto loadTextureByIndex = [&textureMetadatas, &allMipMetadata, &textureData, &context, &transferQueue,
    &transferCommandPool](int32_t textureIndex, resources::UniqueAllocatedImage& image,
    vk::UniqueImageView& imageView) -> vk::Result
    {
        if (textureIndex < 0)
        {
            return vk::Result::eErrorInitializationFailed;
        }
        const auto index = static_cast<uint32_t>(textureIndex);
        if (index >= textureMetadatas.size())
        {
            return vk::Result::eErrorInitializationFailed;
        }
        const TextureMetadata& metadata = textureMetadatas[index];
        if ((metadata.mipTableOffset % sizeof(TextureMipMetadata)) != 0)
        {
            return vk::Result::eErrorInitializationFailed;
        }
        const uint64_t firstMipIndex = metadata.mipTableOffset / sizeof(TextureMipMetadata);
        const uint64_t mipEntryCount = static_cast<uint64_t>(metadata.mipCount);
        if (firstMipIndex + mipEntryCount > allMipMetadata.size())
        {
            return vk::Result::eErrorInitializationFailed;
        }
        auto textureMipMetadata =
        allMipMetadata.subspan(static_cast<size_t>(firstMipIndex), static_cast<size_t>(mipEntryCount));
        return uploadEnvironmentTexture(context.device, transferQueue, transferCommandPool, context.allocator, metadata,
        textureMipMetadata, textureData, image, imageView);
    };
    // ========================================================
    // Upload skybox
    // ========================================================
    if (auto result =
    loadTextureByIndex(environmentInfo.skyboxTextureIndex, resources.skyboxImage, resources.skyboxImageView);
    result != vk::Result::eSuccess)
    {
        std::cerr << "[Environment] Failed to upload skybox." << std::endl;
        return {result, {}};
    }
    // ========================================================
    // Upload irradiance
    // ========================================================
    if (auto result = loadTextureByIndex(environmentInfo.irradianceTextureIndex, resources.irradianceImage,
    resources.irradianceImageView);
    result != vk::Result::eSuccess)
    {
        std::cerr << "[Environment] Failed to upload irradiance." << std::endl;
        return {result, {}};
    }
    // ========================================================
    // Upload prefiltered radiance
    // ========================================================
    if (auto result = loadTextureByIndex(environmentInfo.prefilteredTextureIndex, resources.radianceImage,
    resources.radianceImageView);
    result != vk::Result::eSuccess)
    {
        std::cerr << "[Environment] Failed to upload radiance." << std::endl;
        return {result, {}};
    }
    // ========================================================
    // Allocate descriptor set
    // ========================================================
    vk::DescriptorSetLayout environmentSetLayout = *rendererResources.environmentSetLayout;
    vk::DescriptorSetAllocateInfo allocateInfo{
        .descriptorPool = *context.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &environmentSetLayout};

    auto [allocateResult, sets] = context.device.allocateDescriptorSets(allocateInfo);
    if (allocateResult != vk::Result::eSuccess)
    {
        return {allocateResult, {}};
    }
    resources.environmentSet = sets.front();
    // ========================================================
    // Update descriptor set
    // ========================================================
    vk::DescriptorImageInfo skyboxInfo{
        .sampler = nullptr,
        .imageView = *resources.skyboxImageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    vk::DescriptorImageInfo irradianceInfo{
        .sampler = nullptr,
        .imageView = *resources.irradianceImageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::DescriptorImageInfo radianceInfo{
        .sampler = nullptr,
        .imageView = *resources.radianceImageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    std::array writes{vk::WriteDescriptorSet{
            .dstSet = resources.environmentSet,
            .dstBinding = static_cast<uint32_t>(EnvironmentBindings::eSkyboxImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &skyboxInfo},

        vk::WriteDescriptorSet{
            .dstSet = resources.environmentSet,
            .dstBinding = static_cast<uint32_t>(EnvironmentBindings::eIrradianceImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &irradianceInfo},
        vk::WriteDescriptorSet{
            .dstSet = resources.environmentSet,
            .dstBinding = static_cast<uint32_t>(EnvironmentBindings::eRadianceImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &radianceInfo}};
    context.device.updateDescriptorSets(writes, {});
    std::cout << "[Environment] Environment resources created." << std::endl;
    return {vk::Result::eSuccess, std::move(resources)};
}
} // namespace shuttle::engine::render
