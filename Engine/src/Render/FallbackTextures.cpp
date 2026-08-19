#include "FallbackTextures.hpp"

#include <vector>

namespace shuttle::engine::render
{
    namespace
    {
        struct UploadedImage2D
        {
            resources::UniqueAllocatedImage image;
            vk::UniqueImageView imageView;
        };

        [[nodiscard]]
        vk::Result uploadImage2D(
            vk::Device device,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator,
            vk::Format format,
            uint32_t width,
            uint32_t height,
            uint32_t mipCount,
            std::span<const std::byte> imageBytes,
            std::span<const vk::BufferImageCopy> copyRegions,
            UploadedImage2D& outImage)
        {
            // ============================================================
            // Staging Buffer
            // ============================================================

            auto [createStagingResult, stagingBuffer] = allocator.createAndAllocateBufferUnique({
                .size = imageBytes.size_bytes(),
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive
            }, resources::MemoryUsage::eCpuOnly);

            if (createStagingResult != vk::Result::eSuccess) return createStagingResult;

            if (auto writeResult = allocator.writeBufferFromHost({
                .dstBuffer = *stagingBuffer,
                .dstBufferOffset = 0,
                .srcData = imageBytes.data(),
                .dataSize = imageBytes.size_bytes()
            }); writeResult != vk::Result::eSuccess)
            {
                return writeResult;
            }

            // ============================================================
            // Image
            // ============================================================

            auto [createImageResult, image] =
                allocator.createAndAllocateImageUnique(
                    vk::ImageCreateInfo{
                        .imageType = vk::ImageType::e2D,
                        .format = format,
                        .extent = vk::Extent3D{
                            .width = width,
                            .height = height,
                            .depth = 1
                        },
                        .mipLevels = mipCount,
                        .arrayLayers = 1,
                        .samples = vk::SampleCountFlagBits::e1,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage =
                            vk::ImageUsageFlagBits::eTransferDst |
                            vk::ImageUsageFlagBits::eSampled,
                        .sharingMode = vk::SharingMode::eExclusive,
                        .initialLayout = vk::ImageLayout::eUndefined
                    },
                    resources::MemoryUsage::eGpuOnly);

            if (createImageResult != vk::Result::eSuccess) return createImageResult;

            // ============================================================
            // Image View
            // ============================================================

            auto [createViewResult, imageView] =
                device.createImageViewUnique(
                    vk::ImageViewCreateInfo{
                        .image = *image,
                        .viewType = vk::ImageViewType::e2D,
                        .format = format,
                        .subresourceRange =
                            vk::ImageSubresourceRange{
                                .aspectMask =
                                    vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = mipCount,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            }
                    });

            if (createViewResult != vk::Result::eSuccess) return createViewResult;

            // ============================================================
            // One-time Command Buffer
            // ============================================================

            auto [allocateCommandBufferResult, commandBuffers] = device.allocateCommandBuffersUnique({
                .commandPool = transferCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1
            });

            if (allocateCommandBufferResult != vk::Result::eSuccess) return allocateCommandBufferResult;

            vk::CommandBuffer cmd =
                commandBuffers.front().get();

            if (auto beginResult = cmd.begin({
                .flags =
                    vk::CommandBufferUsageFlagBits::eOneTimeSubmit
                }); beginResult != vk::Result::eSuccess)
            {
                return beginResult;
            }

            auto transitionImage =
                [&](vk::Image image_,
                    vk::ImageLayout oldLayout,
                    vk::ImageLayout newLayout,
                    vk::AccessFlags srcAccess,
                    vk::AccessFlags dstAccess,
                    vk::PipelineStageFlags srcStage,
                    vk::PipelineStageFlags dstStage)
                {
                    vk::ImageMemoryBarrier barrier{
                        .srcAccessMask = srcAccess,
                        .dstAccessMask = dstAccess,
                        .oldLayout = oldLayout,
                        .newLayout = newLayout,
                        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                        .image = image_,
                        .subresourceRange =
                            vk::ImageSubresourceRange{
                                .aspectMask =
                                    vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = mipCount,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            }
                    };

                    cmd.pipelineBarrier(
                        srcStage,
                        dstStage,
                        {},
                        nullptr,
                        nullptr,
                        barrier);
                };

            transitionImage(
                *image,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                {},
                vk::AccessFlagBits::eTransferWrite,
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer);

            cmd.copyBufferToImage(
                *stagingBuffer,
                *image,
                vk::ImageLayout::eTransferDstOptimal,
                copyRegions);

            transitionImage(
                *image,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::AccessFlagBits::eTransferWrite,
                {},
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eBottomOfPipe);

            if (auto endResult = cmd.end(); endResult != vk::Result::eSuccess) {
                return endResult;
            }

            vk::SubmitInfo submitInfo{
                .commandBufferCount = 1,
                .pCommandBuffers = &cmd
            };

            if (auto submitResult =
                    transferQueue.submit(
                        1,
                        &submitInfo,
                        nullptr);
                submitResult != vk::Result::eSuccess)
            {
                return submitResult;
            }

            if (auto waitResult = transferQueue.waitIdle(); waitResult != vk::Result::eSuccess) {
                return waitResult;
            }

            outImage.image = std::move(image);
            outImage.imageView = std::move(imageView);

            return vk::Result::eSuccess;
        }

        [[nodiscard]]
        vk::Result createFallbackTexture1x1(
            vk::Device device,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator,
            std::array<std::byte, 4> rgba,
            resources::UniqueAllocatedImage& outImage,
            vk::UniqueImageView& outImageView)
        {
            vk::BufferImageCopy copyRegion{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                    vk::ImageSubresourceLayers{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    },
                .imageOffset = vk::Offset3D{0, 0, 0},
                .imageExtent = vk::Extent3D{1, 1, 1}
            };

            UploadedImage2D uploaded{};

            const vk::Result uploadResult =
                uploadImage2D(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    vk::Format::eR8G8B8A8Unorm,
                    1,
                    1,
                    1,
                    std::span<const std::byte>(
                        rgba.data(),
                        rgba.size()),
                    std::span<const vk::BufferImageCopy>(
                        &copyRegion,
                        1),
                    uploaded);

            if (uploadResult != vk::Result::eSuccess) return uploadResult;

            outImage = std::move(uploaded.image);
            outImageView = std::move(uploaded.imageView);

            return vk::Result::eSuccess;
        }
    }

    vk::ResultValue<FallbackTextures> createFallbackTextures(
        vk::Device device,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator,
        DescriptorHeapSet& descriptorHeap)
    {
        FallbackTextures fallbackTextures{};

        // ============================================================
        // Albedo fallback
        // ============================================================
        //
        // White RGBA.

        if (const vk::Result createResult =
                createFallbackTexture1x1(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    std::array{
                        std::byte{255},
                        std::byte{255},
                        std::byte{255},
                        std::byte{255}
                    },
                    fallbackTextures.storage.albedoTexture.image,
                    fallbackTextures.storage.albedoTexture.imageView);
            createResult != vk::Result::eSuccess)
        {
            return {createResult, {}};
        }

        auto [writeAlbedoResult, albedoIndex] = descriptorHeap.writeTextureUnique(
            *fallbackTextures.storage.albedoTexture.imageView,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        if (writeAlbedoResult != vk::Result::eSuccess) return {writeAlbedoResult, {}};

        fallbackTextures.indices.albedo = albedoIndex.get();

        fallbackTextures.storage.albedoTexture.descriptorSlot = std::move(albedoIndex);


        // ============================================================
        // Normal fallback
        // ============================================================
        //
        // Tangent-space flat normal:
        // RGB = (0.5, 0.5, 1.0)

        if (const vk::Result createResult =
                createFallbackTexture1x1(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    std::array{
                        std::byte{128},
                        std::byte{128},
                        std::byte{255},
                        std::byte{255}
                    },
                    fallbackTextures.storage.normalTexture.image,
                    fallbackTextures.storage.normalTexture.imageView);
            createResult != vk::Result::eSuccess)
        {
            return {createResult, {}};
        }

        auto [writeNormalResult, normalIndex] = descriptorHeap.writeTextureUnique(
            *fallbackTextures.storage.normalTexture.imageView,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        if (writeNormalResult != vk::Result::eSuccess) return {writeNormalResult, {}};

        fallbackTextures.indices.normal = normalIndex.get();
        fallbackTextures.storage.normalTexture.descriptorSlot = std::move(normalIndex);

        // ============================================================
        // ORM fallback
        // ============================================================
        //
        // R = AO        = 1.0
        // G = Roughness = 1.0
        // B = Metallic  = 0.0
        // A = unused    = 1.0

        if (const vk::Result createResult =
                createFallbackTexture1x1(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    std::array{
                        std::byte{255},
                        std::byte{255},
                        std::byte{0},
                        std::byte{255}
                    },
                    fallbackTextures.storage.ormTexture.image,
                    fallbackTextures.storage.ormTexture.imageView);
            createResult != vk::Result::eSuccess)
        {
            return {createResult, {}};
        }

        auto [writeOrmResult, ormIndex] = descriptorHeap.writeTextureUnique(
            *fallbackTextures.storage.ormTexture.imageView,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        if (writeOrmResult != vk::Result::eSuccess) return {writeOrmResult, {}};

        fallbackTextures.indices.orm = ormIndex.get();
        fallbackTextures.storage.ormTexture.descriptorSlot = std::move(ormIndex);

        // ============================================================
        // Emission fallback
        // ============================================================
        //
        // Black RGBA.

        if (const vk::Result createResult =
                createFallbackTexture1x1(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    std::array{
                        std::byte{0},
                        std::byte{0},
                        std::byte{0},
                        std::byte{255}
                    },
                    fallbackTextures.storage.emissionTexture.image,
                    fallbackTextures.storage.emissionTexture.imageView);
            createResult != vk::Result::eSuccess)
        {
            return {createResult, {}};
        }

        auto [writeEmissionResult, emissionIndex] = descriptorHeap.writeTextureUnique(
            *fallbackTextures.storage.emissionTexture.imageView,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        if (writeEmissionResult != vk::Result::eSuccess) return {writeEmissionResult, {}};

        fallbackTextures.indices.emission = emissionIndex.get();
        fallbackTextures.storage.emissionTexture.descriptorSlot = std::move(emissionIndex);

        return { vk::Result::eSuccess,std::move(fallbackTextures) };
    }
}