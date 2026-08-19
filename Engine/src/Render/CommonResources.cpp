#include "CommonResources.hpp"

#include "tinyddsloader.h"

#include <vector>
#include <span>
#include <cstring>
#include <iostream>
#include <cassert>

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
            auto [createStagingResult, stagingBuffer] =
                allocator.createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = imageBytes.size_bytes(),
                        .usage = vk::BufferUsageFlagBits::eTransferSrc,
                        .sharingMode = vk::SharingMode::eExclusive
                    },
                    resources::MemoryUsage::eCpuOnly);

            if (createStagingResult != vk::Result::eSuccess)
            {
                return createStagingResult;
            }

            if (auto writeResult =
                    allocator.writeBufferFromHost(
                        resources::CopyHostToBufferInfo{
                            .dstBuffer = *stagingBuffer,
                            .dstBufferOffset = 0,
                            .srcData = imageBytes.data(),
                            .dataSize = imageBytes.size_bytes()
                        });
                writeResult != vk::Result::eSuccess)
            {
                return writeResult;
            }

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

            if (createImageResult != vk::Result::eSuccess)
                     {
                return createImageResult;
            }

           auto [createImageViewResult, imageView] =
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

            if (createImageViewResult != vk::Result::eSuccess)
            {
                return createImageViewResult;
            }

           auto [allocateCommandBufferResult, commandBuffers] =
               device.allocateCommandBuffersUnique(
                    vk::CommandBufferAllocateInfo{
                      .commandPool = transferCommandPool,
                        .level = vk::CommandBufferLevel::ePrimary,
                        .commandBufferCount = 1
                    });

            if (allocateCommandBufferResult != vk::Result::eSuccess)
            {
                return allocateCommandBufferResult;
            }

            vk::CommandBuffer cmd =
                commandBuffers.front().get();

            if (auto beginResult =
                    cmd.begin(
                        vk::CommandBufferBeginInfo{
                            .flags =
                                vk::CommandBufferUsageFlagBits::eOneTimeSubmit
                        });
                beginResult != vk::Result::eSuccess)
            {
                return beginResult;
            }

            auto transitionImage = [&cmd, mipCount](
                    vk::Image imageHandle,
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
                        .image = imageHandle,
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

            if (auto endResult = cmd.end();
                endResult != vk::Result::eSuccess)
            {
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

            if (auto waitResult = transferQueue.waitIdle();
                waitResult != vk::Result::eSuccess)
            {
                return waitResult;
            }

            outImage.image = std::move(image);
            outImage.imageView = std::move(imageView);

            return vk::Result::eSuccess;
        }

        [[nodiscard]]
        vk::Result uploadBufferDeviceLocal(
            vk::Device device,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator,
            resources::UniqueAllocatedBuffer& dstBuffer,
            void const* data,
            vk::DeviceSize dataSize)
        {
            auto [createStagingResult, stagingBuffer] =
                allocator.createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = dataSize,
                        .usage = vk::BufferUsageFlagBits::eTransferSrc,
                        .sharingMode = vk::SharingMode::eExclusive
                    },
                    resources::MemoryUsage::eCpuOnly);

            if (createStagingResult != vk::Result::eSuccess)
            {
                return createStagingResult;
            }

            if (auto writeResult =
                    allocator.writeBufferFromHost(
                        resources::CopyHostToBufferInfo{
                            .dstBuffer = *stagingBuffer,
                            .dstBufferOffset = 0,
                            .srcData = data,
                            .dataSize = dataSize
                        });
                writeResult != vk::Result::eSuccess)
            {
                return writeResult;
            }

            auto [allocateCommandBufferResult, commandBuffers] =
                device.allocateCommandBuffersUnique(
                    vk::CommandBufferAllocateInfo{
                        .commandPool = transferCommandPool,
                        .level = vk::CommandBufferLevel::ePrimary,
                        .commandBufferCount = 1
                    });

            if (allocateCommandBufferResult != vk::Result::eSuccess)
            {
                return allocateCommandBufferResult;
            }

            vk::CommandBuffer cmd =
                commandBuffers.front().get();

            if (auto beginResult =
                    cmd.begin(
                        vk::CommandBufferBeginInfo{
                            .flags =
                                vk::CommandBufferUsageFlagBits::eOneTimeSubmit
                        });
                beginResult != vk::Result::eSuccess)
            {
                return beginResult;
            }

            cmd.copyBuffer(
                *stagingBuffer,
                *dstBuffer,
                vk::BufferCopy{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = dataSize
                });

            vk::BufferMemoryBarrier barrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .buffer = *dstBuffer,
                .offset = 0,
                .size = dataSize
            };

            cmd.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eAllCommands,
                {},
                nullptr,
                barrier,
                nullptr);

            if (auto endResult = cmd.end();
                endResult != vk::Result::eSuccess)
            {
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

            if (auto waitResult = transferQueue.waitIdle();
                waitResult != vk::Result::eSuccess)
            {
                return waitResult;
            }

            return vk::Result::eSuccess;
        }

        [[nodiscard]]
        vk::Result loadBrdfLutDds(
            vk::Device device,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator,
            std::filesystem::path const& brdfLutPath,
            resources::UniqueAllocatedImage& outImage,
            vk::UniqueImageView& outImageView)
        {
            std::cout
                << "[CommonResources] Loading BRDF LUT: "
                << brdfLutPath.string()
                << std::endl;

            tinyddsloader::DDSFile ddsFile;

            const std::string pathString =
                brdfLutPath.string();

            const auto loadResult =
                ddsFile.Load(pathString.c_str());

            if (loadResult != tinyddsloader::Result::Success)
            {
                std::cerr
                    << "[CommonResources] Failed to load BRDF LUT DDS."
                    << std::endl;

                return vk::Result::eErrorInitializationFailed;
            }

            assert(
                ddsFile.GetFormat() ==
                tinyddsloader::DDSFile::DXGIFormat::R16G16_Float);

            constexpr vk::Format Format =
                vk::Format::eR16G16Sfloat;

            const uint32_t width =
                ddsFile.GetWidth();

            const uint32_t height =
                ddsFile.GetHeight();

            const uint32_t mipCount =
                ddsFile.GetMipCount();

            vk::DeviceSize totalSize = 0;

            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                const auto* imageData =
                    ddsFile.GetImageData(mip, 0);

                totalSize += imageData->m_memSlicePitch;
            }

            std::vector<std::byte> textureBytes;
            textureBytes.resize(
                static_cast<size_t>(totalSize));

            std::vector<vk::BufferImageCopy> regions;
            regions.reserve(mipCount);

            std::byte* dst =
                textureBytes.data();

            vk::DeviceSize offset = 0;

            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                const auto* imageData =
                    ddsFile.GetImageData(mip, 0);

                std::memcpy(
                    dst,
                    imageData->m_mem,
                    imageData->m_memSlicePitch);

                dst += imageData->m_memSlicePitch;

                const uint32_t mipWidth =
                    std::max(1u, width >> mip);

                const uint32_t mipHeight =
                    std::max(1u, height >> mip);

                regions.push_back(
                    vk::BufferImageCopy{
                        .bufferOffset = offset,
                        .bufferRowLength = 0,
                        .bufferImageHeight = 0,
                        .imageSubresource =
                            vk::ImageSubresourceLayers{
                                .aspectMask =
                                    vk::ImageAspectFlagBits::eColor,
                                .mipLevel = mip,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            },
                        .imageOffset =
                            vk::Offset3D{0, 0, 0},
                        .imageExtent =
                            vk::Extent3D{
                                .width = mipWidth,
                                .height = mipHeight,
                                .depth = 1
                            }
                    });

                offset += imageData->m_memSlicePitch;
            }

            UploadedImage2D uploaded{};

            const vk::Result uploadResult =
                uploadImage2D(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    Format,
                    width,
                    height,
                    mipCount,
                    std::span<const std::byte>(
                        textureBytes.data(),
                        textureBytes.size()),
                    std::span<const vk::BufferImageCopy>(
                        regions.data(),
                        regions.size()),
                    uploaded);

            if (uploadResult != vk::Result::eSuccess)
            {
                return uploadResult;
            }

            outImage = std::move(uploaded.image);
            outImageView = std::move(uploaded.imageView);

            std::cout
                << "[CommonResources] BRDF LUT uploaded: "
                << width
                << "x"
                << height
                << " mips="
                << mipCount
                << " bytes="
                << totalSize
                << std::endl;

            return vk::Result::eSuccess;
        }

        [[nodiscard]]
        vk::ResultValue<vk::UniqueSampler> createMaterialSampler(
            vk::Device device)
        {
            return device.createSamplerUnique(
                vk::SamplerCreateInfo{
                    .magFilter = vk::Filter::eLinear,
                    .minFilter = vk::Filter::eLinear,
                    .mipmapMode = vk::SamplerMipmapMode::eLinear,
                    .addressModeU = vk::SamplerAddressMode::eRepeat,
                    .addressModeV = vk::SamplerAddressMode::eRepeat,
                    .addressModeW = vk::SamplerAddressMode::eRepeat,
                    .mipLodBias = 0.0f,
                    .anisotropyEnable = vk::True,
                    .maxAnisotropy = 16.0f,
                    .compareEnable = vk::False,
                    .compareOp = vk::CompareOp::eAlways,
                    .minLod = 0.0f,
                    .maxLod = VK_LOD_CLAMP_NONE,
                    .borderColor = vk::BorderColor::eIntOpaqueBlack,
                    .unnormalizedCoordinates = vk::False
                });
        }

        [[nodiscard]]
        vk::ResultValue<vk::UniqueSampler> createNearestSampler(
            vk::Device device)
        {
            return device.createSamplerUnique(
                vk::SamplerCreateInfo{
                    .magFilter = vk::Filter::eNearest,
                    .minFilter = vk::Filter::eNearest,
                    .mipmapMode = vk::SamplerMipmapMode::eNearest,
                    .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vk::SamplerAddressMode::eClampToEdge,
                    .mipLodBias = 0.0f,
                    .anisotropyEnable = vk::False,
                    .maxAnisotropy = 1.0f,
                    .compareEnable = vk::False,
                    .compareOp = vk::CompareOp::eAlways,
                    .minLod = 0.0f,
                    .maxLod = VK_LOD_CLAMP_NONE,
                    .borderColor = vk::BorderColor::eIntOpaqueBlack,
                    .unnormalizedCoordinates = vk::False
                });
        }

        [[nodiscard]]
        vk::ResultValue<vk::UniqueSampler> createShadowSampler(
            vk::Device device)
        {
            return device.createSamplerUnique(
                vk::SamplerCreateInfo{
                    .magFilter = vk::Filter::eLinear,
                    .minFilter = vk::Filter::eLinear,
                    .mipmapMode = vk::SamplerMipmapMode::eNearest,
                    .addressModeU = vk::SamplerAddressMode::eClampToBorder,
                    .addressModeV = vk::SamplerAddressMode::eClampToBorder,
                    .addressModeW = vk::SamplerAddressMode::eClampToBorder,
                    .mipLodBias = 0.0f,
                    .anisotropyEnable = vk::False,
                    .maxAnisotropy = 1.0f,
                    .compareEnable = vk::False,
                    .compareOp = vk::CompareOp::eAlways,
                    .minLod = 0.0f,
                    .maxLod = 0.0f,
                    .borderColor = vk::BorderColor::eFloatOpaqueWhite,
                    .unnormalizedCoordinates = vk::False
                });
        }
    }

    vk::ResultValue<CommonResources> createCommonResources(
        vk::Device device,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator,
        DescriptorHeapSet& descriptorHeap,
        std::filesystem::path const& brdfLutPath)
    {
        CommonResources common{};

        // ============================================================
        // Samplers
        // ============================================================

        auto [createMaterialSamplerResult, materialSampler] =
            createMaterialSampler(device);

        if (createMaterialSamplerResult != vk::Result::eSuccess)
        {
            return {createMaterialSamplerResult, {}};
        }

        auto [createNearestSamplerResult, nearestSampler] =
            createNearestSampler(device);

        if (createNearestSamplerResult != vk::Result::eSuccess)
        {
            return {createNearestSamplerResult, {}};
        }

        auto [createShadowSamplerResult, shadowSampler] =
            createShadowSampler(device);

        if (createShadowSamplerResult != vk::Result::eSuccess)
        {
            return {createShadowSamplerResult, {}};
        }

        common.storage.materialSampler =
            std::move(materialSampler);

        common.storage.nearestSampler =
            std::move(nearestSampler);

        common.storage.shadowSampler =
            std::move(shadowSampler);

        // ============================================================
        // Write samplers into descriptor heap set
        // ============================================================

        auto [writeMaterialSamplerResult, materialSamplerIndex] =
            descriptorHeap.writeSamplerUnique(
                *common.storage.materialSampler);

        if (writeMaterialSamplerResult != vk::Result::eSuccess)
        {
            return {writeMaterialSamplerResult, {}};
        }

        common.info.materialSampler =
            materialSamplerIndex.get();

        common.storage.materialSamplerSlot =
            std::move(materialSamplerIndex);

        auto [writeNearestSamplerResult, nearestSamplerIndex] =
            descriptorHeap.writeSamplerUnique(
                *common.storage.nearestSampler);

        if (writeNearestSamplerResult != vk::Result::eSuccess)
        {
            return {writeNearestSamplerResult, {}};
        }

        common.info.nearestSampler =
            nearestSamplerIndex.get();

        common.storage.nearestSamplerSlot =
            std::move(nearestSamplerIndex);

        auto [writeShadowSamplerResult, shadowSamplerIndex] =
            descriptorHeap.writeSamplerUnique(
                *common.storage.shadowSampler);

        if (writeShadowSamplerResult != vk::Result::eSuccess)
        {
            return {writeShadowSamplerResult, {}};
        }

        common.info.shadowSampler =
            shadowSamplerIndex.get();

        common.storage.shadowSamplerSlot =
            std::move(shadowSamplerIndex);

        // ============================================================
        // BRDF LUT
        // ============================================================

        if (const vk::Result loadBrdfResult =
                loadBrdfLutDds(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    brdfLutPath,
                    common.storage.brdfLutTexture.image,
                    common.storage.brdfLutTexture.imageView);
            loadBrdfResult != vk::Result::eSuccess)
        {
            return {loadBrdfResult, {}};
        }

        auto [writeBrdfResult, brdfIndex] =
            descriptorHeap.writeTextureUnique(
                *common.storage.brdfLutTexture.imageView,
                vk::ImageLayout::eShaderReadOnlyOptimal);

        if (writeBrdfResult != vk::Result::eSuccess)
        {
            return {writeBrdfResult, {}};
        }

        common.info.brdfLutTexture =
            brdfIndex.get();

        common.storage.brdfLutTexture.descriptorSlot =
            std::move(brdfIndex);

        // ============================================================
        // CommonResourcesInfo GPU Buffer
        // ============================================================

        auto [createInfoBufferResult, infoBuffer] =
            allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(CommonResourcesInfo),
                    .usage =
                        vk::BufferUsageFlagBits::eStorageBuffer |
                        vk::BufferUsageFlagBits::eTransferDst |
                        vk::BufferUsageFlagBits::eShaderDeviceAddress,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eGpuOnly);

        if (createInfoBufferResult != vk::Result::eSuccess)
        {
            return {createInfoBufferResult, {}};
        }

        common.infoBuffer =
            std::move(infoBuffer);

        if (const vk::Result uploadInfoResult =
                uploadBufferDeviceLocal(
                    device,
                    transferQueue,
                    transferCommandPool,
                    allocator,
                    common.infoBuffer,
                    &common.info,
                    sizeof(CommonResourcesInfo));
            uploadInfoResult != vk::Result::eSuccess)
        {
            return {uploadInfoResult, {}};
        }

        common.infoAddress =
            device.getBufferAddress(
                vk::BufferDeviceAddressInfo{
                    .buffer = *common.infoBuffer
                });

        return {
            vk::Result::eSuccess,
            std::move(common)
        };
    }
}