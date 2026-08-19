//
// Created by Shagu on 31.07.2026.
//

#include "Render.hpp"

#include <Assets/Core/BlobView.hpp>
#include <Assets/Core/BlobFormat.hpp>
#include <Assets/Formats/Environment.hpp>
#include <Assets/Formats/Texture.hpp>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include "DescriptorHeapSet.hpp"
#include "SceneData.hpp"

namespace shuttle::engine::render
{
    namespace
    {
        vk::ImageType toVkImageType(
            assets::formats::texture::ImageType type)
        {
            using T = assets::formats::texture::ImageType;

            switch (type)
            {
            case T::Image1D:
                return vk::ImageType::e1D;

            case T::Image2D:
                return vk::ImageType::e2D;

            case T::Image3D:
                return vk::ImageType::e3D;

            default:
                return vk::ImageType::e2D;
            }
        }

        vk::ImageViewType toVkImageViewType(
            assets::formats::texture::ImageViewType type)
        {
            using T = assets::formats::texture::ImageViewType;

            switch (type)
            {
            case T::View1D:
                return vk::ImageViewType::e1D;

            case T::View2D:
                return vk::ImageViewType::e2D;

            case T::View3D:
                return vk::ImageViewType::e3D;

            case T::ViewCube:
                return vk::ImageViewType::eCube;

            case T::View1DArray:
                return vk::ImageViewType::e1DArray;

            case T::View2DArray:
                return vk::ImageViewType::e2DArray;

            case T::ViewCubeArray:
                return vk::ImageViewType::eCubeArray;

            default:
                return vk::ImageViewType::e2D;
            }
        }

        bool isCubeViewType(
            assets::formats::texture::ImageViewType type)
        {
            using T = assets::formats::texture::ImageViewType;

            return type == T::ViewCube ||
                   type == T::ViewCubeArray;
        }

        template <typename T>
        std::span<const T> readSectionSpan(
            const assets::core::BlobView& blob,
            assets::core::BlobSectionType sectionType)
        {
            const auto section =
                blob.findSection(sectionType);

            if (!section)
            {
                return {};
            }

            const auto bytes =
                blob.bytes(*section);

            if (bytes.empty())
            {
                return {};
            }

            if (bytes.size_bytes() % sizeof(T) != 0)
            {
                return {};
            }

            return std::span<const T>(
                reinterpret_cast<const T*>(
                    bytes.data()),
                bytes.size_bytes() / sizeof(T));
        }

        std::span<const uint8_t> readSectionBytes(
            const assets::core::BlobView& blob,
            assets::core::BlobSectionType sectionType)
        {
            const auto section =
                blob.findSection(sectionType);

            if (!section)
            {
                return {};
            }

            return blob.bytes(*section);
        }

        vk::ResultValue<vk::UniqueCommandBuffer>
        allocateOneTimeCommandBuffer(
            vk::Device device,
            vk::CommandPool commandPool)
        {
            auto [result, commandBuffers] =
                device.allocateCommandBuffersUnique(
                    vk::CommandBufferAllocateInfo{
                        .commandPool = commandPool,
                        .level = vk::CommandBufferLevel::ePrimary,
                        .commandBufferCount = 1
                    });

            if (result != vk::Result::eSuccess)
            {
                return {result, {}};
            }

            return {
                vk::Result::eSuccess,
                std::move(commandBuffers.front())
            };
        }

        vk::ResultValue<resources::UniqueAllocatedBuffer>
        createDeviceAddressBuffer(
            resources::DeviceAllocator const& allocator,
            vk::DeviceSize size)
        {
            return allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = size == 0 ? 16 : size,
                    .usage =
                        vk::BufferUsageFlagBits::eShaderDeviceAddress |
                        vk::BufferUsageFlagBits::eTransferDst,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eGpuOnly);
        }

        vk::ResultValue<resources::UniqueAllocatedBuffer>
        createStagingBuffer(
            resources::DeviceAllocator const& allocator,
            vk::DeviceSize size)
        {
            return allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlagBits::eMapped);
        }

        vk::Result submitAndWait(
            vk::Device device,
            vk::Queue queue,
            vk::CommandBuffer commandBuffer)
        {
            auto [fenceResult, fence] =
                device.createFenceUnique({});

            if (fenceResult != vk::Result::eSuccess)
            {
                return fenceResult;
            }

            vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
                .commandBuffer = commandBuffer,
                .deviceMask = 0x1
            };

            vk::SubmitInfo2 submitInfo{
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &commandBufferSubmitInfo
            };

            if (auto result = queue.submit2(
                    1,
                    &submitInfo,
                    *fence);
                result != vk::Result::eSuccess)
            {
                return result;
            }

            return device.waitForFences(
                {*fence},
                vk::True,
                UINT64_MAX);
        }

        vk::ResultValue<Texture>
        uploadEnvironmentTexture(
            RenderContext& context,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            DescriptorHeapSet& descriptorHeapSet,
            const assets::formats::texture::TextureMetadata& metadata,
            std::span<const assets::formats::texture::TextureMipMetadata> mipMetadata,
            std::span<const uint8_t> textureData)
        {
            using namespace assets::formats::texture;

            if (metadata.mipCount == 0 ||
                metadata.width == 0 ||
                metadata.height == 0 ||
                metadata.layerCount == 0)
            {
                return {
                    vk::Result::eErrorInitializationFailed,
                    {}
                };
            }

            const auto format =
                static_cast<vk::Format>(
                    metadata.format);

            const vk::ImageType imageType =
                toVkImageType(
                    metadata.imageType);

            const vk::ImageViewType viewType =
                toVkImageViewType(
                    metadata.imageViewType);

            const uint32_t mipCount =
                metadata.mipCount;

            const uint32_t layerCount =
                metadata.layerCount;

            vk::ImageCreateFlags imageFlags{};

            if (isCubeViewType(metadata.imageViewType))
            {
                imageFlags |=
                    vk::ImageCreateFlagBits::eCubeCompatible;
            }

            std::vector<std::byte> uploadBytes;
            std::vector<vk::BufferImageCopy2> copyRegions;

            size_t totalSize = 0;

            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                totalSize += static_cast<size_t>(
                    mipMetadata[mip].dataSize);
            }

            uploadBytes.reserve(totalSize);

            copyRegions.reserve(mipCount);

            vk::DeviceSize bufferOffset = 0;

            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                if (mip >= mipMetadata.size())
                {
                    return {
                        vk::Result::eErrorInitializationFailed,
                        {}
                    };
                }

                const auto& mipInfo =
                    mipMetadata[mip];

                if (mipInfo.width == 0 ||
                    mipInfo.height == 0 ||
                    mipInfo.dataSize == 0)
                {
                    return {
                        vk::Result::eErrorInitializationFailed,
                        {}
                    };
                }

                if (mipInfo.dataOffset + mipInfo.dataSize >
                    textureData.size())
                {
                    return {
                        vk::Result::eErrorInitializationFailed,
                        {}
                    };
                }

                const uint8_t* source =
                    textureData.data() +
                    mipInfo.dataOffset;

                const auto dataSize =
                    static_cast<size_t>(
                        mipInfo.dataSize);

                const size_t oldSize =
                    uploadBytes.size();

                uploadBytes.resize(
                    oldSize + dataSize);

                std::memcpy(
                    uploadBytes.data() + oldSize,
                    source,
                    dataSize);

                copyRegions.push_back(
                    vk::BufferImageCopy2{
                        .bufferOffset = bufferOffset,
                        .bufferRowLength = 0,
                        .bufferImageHeight = 0,
                        .imageSubresource =
                            vk::ImageSubresourceLayers{
                                .aspectMask =
                                    vk::ImageAspectFlagBits::eColor,
                                .mipLevel = mip,
                                .baseArrayLayer = 0,
                                .layerCount = layerCount
                            },
                        .imageOffset =
                            vk::Offset3D{0, 0, 0},
                        .imageExtent =
                            vk::Extent3D{
                                mipInfo.width,
                                mipInfo.height,
                                1
                            }
                    });

                bufferOffset +=
                    mipInfo.dataSize;
            }

            if (uploadBytes.empty() ||
                copyRegions.empty())
            {
                return {
                    vk::Result::eErrorInitializationFailed,
                    {}
                };
            }

            auto [stagingResult, stagingBuffer] =
                createStagingBuffer(
                    context.allocator,
                    uploadBytes.size());

            if (stagingResult != vk::Result::eSuccess)
            {
                return {stagingResult, {}};
            }

            void* stagingData =
                context.allocator.getMappedPointer(
                    *stagingBuffer);

            std::memcpy(
                stagingData,
                uploadBytes.data(),
                uploadBytes.size());

            auto [imageResult, image] =
                context.allocator.createAndAllocateImageUnique(
                    vk::ImageCreateInfo{
                        .flags = imageFlags,
                        .imageType = imageType,
                        .format = format,
                        .extent =
                            vk::Extent3D{
                                metadata.width,
                                metadata.height,
                                metadata.depth
                            },
                        .mipLevels = mipCount,
                        .arrayLayers = layerCount,
                        .samples = vk::SampleCountFlagBits::e1,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage =
                            vk::ImageUsageFlagBits::eTransferDst |
                            vk::ImageUsageFlagBits::eSampled,
                        .sharingMode = vk::SharingMode::eExclusive,
                        .initialLayout = vk::ImageLayout::eUndefined
                    },
                    resources::MemoryUsage::eGpuOnly);

            if (imageResult != vk::Result::eSuccess)
            {
                return {imageResult, {}};
            }

            auto [imageViewResult, imageView] =
                context.device.createImageViewUnique(
                    vk::ImageViewCreateInfo{
                        .image = *image,
                        .viewType = viewType,
                        .format = format,
                        .subresourceRange =
                            vk::ImageSubresourceRange{
                                .aspectMask =
                                    vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = mipCount,
                                .baseArrayLayer = 0,
                                .layerCount = layerCount
                            }
                    });

            if (imageViewResult != vk::Result::eSuccess)
            {
                return {imageViewResult, {}};
            }

            auto [commandBufferResult, commandBufferUnique] =
                allocateOneTimeCommandBuffer(
                    context.device,
                    transferCommandPool);

            if (commandBufferResult != vk::Result::eSuccess)
            {
                return {commandBufferResult, {}};
            }

            vk::CommandBuffer commandBuffer =
                *commandBufferUnique;

            if (auto result =
                    commandBuffer.begin(
                        vk::CommandBufferBeginInfo{
                            .flags =
                                vk::CommandBufferUsageFlagBits::
                                    eOneTimeSubmit
                        });
                result != vk::Result::eSuccess)
            {
                return {result, {}};
            }

            vk::ImageMemoryBarrier2 toTransferDst{
                .srcStageMask =
                    vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask =
                    vk::AccessFlagBits2::eNone,
                .dstStageMask =
                    vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask =
                    vk::AccessFlagBits2::eTransferWrite,
                .oldLayout =
                    vk::ImageLayout::eUndefined,
                .newLayout =
                    vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex =
                    vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex =
                    vk::QueueFamilyIgnored,
                .image = *image,
                .subresourceRange =
                    vk::ImageSubresourceRange{
                        .aspectMask =
                            vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = mipCount,
                        .baseArrayLayer = 0,
                        .layerCount = layerCount
                    }
            };

            commandBuffer.pipelineBarrier2(
                vk::DependencyInfo{
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers =
                        &toTransferDst
                });

            vk::CopyBufferToImageInfo2 copyInfo{
                .srcBuffer = *stagingBuffer,
                .dstImage = *image,
                .dstImageLayout =
                    vk::ImageLayout::eTransferDstOptimal,
                .regionCount =
                    static_cast<uint32_t>(
                        copyRegions.size()),
                .pRegions = copyRegions.data()
            };

            commandBuffer.copyBufferToImage2(copyInfo);

            vk::ImageMemoryBarrier2 toShaderRead{
                .srcStageMask =
                    vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask =
                    vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask =
                    vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask =
                    vk::AccessFlagBits2::eShaderRead,
                .oldLayout =
                    vk::ImageLayout::eTransferDstOptimal,
                .newLayout =
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex =
                    vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex =
                    vk::QueueFamilyIgnored,
                .image = *image,
                .subresourceRange =
                    vk::ImageSubresourceRange{
                        .aspectMask =
                            vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = mipCount,
                        .baseArrayLayer = 0,
                        .layerCount = layerCount
                    }
            };

            commandBuffer.pipelineBarrier2(
                vk::DependencyInfo{
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers =
                        &toShaderRead
                });

            if (auto result = commandBuffer.end();
                result != vk::Result::eSuccess)
            {
                return {result, {}};
            }

            if (auto result =
                    submitAndWait(
                        context.device,
                        transferQueue,
                        commandBuffer);
                result != vk::Result::eSuccess)
            {
                return {result, {}};
            }

            vk::ResultValue<UniqueDescriptorSlot> descriptorResult = descriptorHeapSet.writeTextureUnique(
                  *imageView,
                  vk::ImageLayout::eShaderReadOnlyOptimal);

            auto [writeDescriptorResult, descriptorSlot] =
                std::move(descriptorResult);

            if (writeDescriptorResult != vk::Result::eSuccess)
            {
                return {writeDescriptorResult, {}};
            }

            return {
                vk::Result::eSuccess,
                Texture{
                    .image = std::move(image),
                    .imageView = std::move(imageView),
                    .descriptorSlot = std::move(descriptorSlot)
                }
            };
        }

        vk::ResultValue<Texture>
        loadEnvironmentTextureByIndex(
            RenderContext& context,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            DescriptorHeapSet& descriptorHeapSet,
            std::span<const assets::formats::texture::TextureMetadata> textureMetadatas,
            std::span<const assets::formats::texture::TextureMipMetadata> allMipMetadata,
            std::span<const uint8_t> textureData,
            int32_t textureIndex)
        {
            using namespace assets::formats::texture;

            if (textureIndex < 0)
            {
                return {
                    vk::Result::eErrorInitializationFailed,
                    {}
                };
            }

            const auto index =
                static_cast<uint32_t>(
                    textureIndex);

            if (index >= textureMetadatas.size())
            {
                return {
                    vk::Result::eErrorInitializationFailed,
                    {}
                };
            }

            const TextureMetadata& metadata =
                textureMetadatas[index];

            if ((metadata.mipTableOffset %
                 sizeof(TextureMipMetadata)) != 0)
            {
                return {
                    vk::Result::eErrorInitializationFailed,
                    {}
                };
            }

            const uint64_t firstMipIndex =
                metadata.mipTableOffset /
                sizeof(TextureMipMetadata);

            const uint64_t mipCount =
                metadata.mipCount;

            if (firstMipIndex + mipCount >
                allMipMetadata.size())
            {
                return {
                    vk::Result::eErrorInitializationFailed,
                    {}
                };
            }

            const auto mipMetadata =
                allMipMetadata.subspan(
                    firstMipIndex,
                    mipCount);

            return uploadEnvironmentTexture(
                context,
                transferQueue,
                transferCommandPool,
                descriptorHeapSet,
                metadata,
                mipMetadata,
                textureData);
        }
    }

    vk::ResultValue<DeviceEnvironmentResources> createEnvironmentResources(
        RenderContext& context,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        DescriptorHeapSet& descriptorHeapSet,
        const std::filesystem::path& environmentBlobPath)
    {
        using namespace assets::core;
        using namespace assets::formats::environment;
        using namespace assets::formats::texture;

        std::cout
            << "[Environment] Loading environment blob: "
            << environmentBlobPath.string()
            << std::endl;

        BlobView blob =
            BlobView::open(
                environmentBlobPath);

        const auto environmentInfos =
            readSectionSpan<EnvironmentInfo>(
                blob,
                BlobSectionType::EnvironmentInfo);

        const auto textureMetadatas =
            readSectionSpan<TextureMetadata>(
                blob,
                BlobSectionType::EnvironmentTextureMetadata);

        const auto mipMetadatas =
            readSectionSpan<TextureMipMetadata>(
                blob,
                BlobSectionType::EnvironmentTextureMipMetadata);

        const auto textureData =
            readSectionBytes(
                blob,
                BlobSectionType::EnvironmentTextureData);

        if (environmentInfos.empty() ||
            textureMetadatas.empty() ||
            mipMetadatas.empty() ||
            textureData.empty())
        {
            std::cerr
                << "[Environment] Missing environment sections."
                << std::endl;

            return {
                vk::Result::eErrorInitializationFailed,
                {}
            };
        }

        const EnvironmentInfo& environmentInfo =
            environmentInfos.front();

        DeviceEnvironmentResources resources{};

        auto [skyboxResult, skybox] =
            loadEnvironmentTextureByIndex(
                context,
                transferQueue,
                transferCommandPool,
                descriptorHeapSet,
                textureMetadatas,
                mipMetadatas,
                textureData,
                environmentInfo.skyboxTextureIndex);

        if (skyboxResult != vk::Result::eSuccess)
        {
            std::cerr
                << "[Environment] Failed to upload skybox."
                << std::endl;

            return {skyboxResult, {}};
        }

        auto [irradianceResult, irradiance] =
            loadEnvironmentTextureByIndex(
                context,
                transferQueue,
                transferCommandPool,
                descriptorHeapSet,
                textureMetadatas,
                mipMetadatas,
                textureData,
                environmentInfo.irradianceTextureIndex);

        if (irradianceResult != vk::Result::eSuccess)
        {
            std::cerr
                << "[Environment] Failed to upload irradiance."
                << std::endl;

            return {irradianceResult, {}};
        }

        auto [radianceResult, radiance] =
            loadEnvironmentTextureByIndex(
                context,
                transferQueue,
                transferCommandPool,
                descriptorHeapSet,
                textureMetadatas,
                mipMetadatas,
                textureData,
                environmentInfo.prefilteredTextureIndex);

        if (radianceResult != vk::Result::eSuccess)
        {
            std::cerr
                << "[Environment] Failed to upload radiance."
                << std::endl;

            return {radianceResult, {}};
        }

        EnvironmentGpuInfo environmentGpuInfo{
            .skyboxTexture = skybox.descriptorSlot.get(),
            .irradianceTexture = irradiance.descriptorSlot.get(),
            .radianceTexture = radiance.descriptorSlot.get(),
            .radianceMipLevels = environmentInfo.prefilteredTextureMipLevels - 1,
        };

        auto [environmentBufferResult, environmentBuffer] =
            createDeviceAddressBuffer(
                context.allocator,
                sizeof(EnvironmentGpuInfo));

        if (environmentBufferResult != vk::Result::eSuccess)
        {
            return {
                environmentBufferResult,
                {}
            };
        }

        auto [stagingResult, stagingBuffer] =
            createStagingBuffer(
                context.allocator,
                sizeof(EnvironmentGpuInfo));

        if (stagingResult != vk::Result::eSuccess)
        {
            return {
                stagingResult,
                {}
            };
        }

        void* stagingData =
            context.allocator.getMappedPointer(
                *stagingBuffer);

        std::memcpy(
            stagingData,
            &environmentGpuInfo,
            sizeof(EnvironmentGpuInfo));

        auto [commandBufferResult, commandBufferUnique] =
            allocateOneTimeCommandBuffer(
                context.device,
                transferCommandPool);

        if (commandBufferResult != vk::Result::eSuccess)
        {
            return {
                commandBufferResult,
                {}
            };
        }

        vk::CommandBuffer commandBuffer =
            *commandBufferUnique;

        if (auto result =
                commandBuffer.begin(
                    vk::CommandBufferBeginInfo{
                        .flags =
                            vk::CommandBufferUsageFlagBits::
                                eOneTimeSubmit
                    });
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        vk::BufferCopy2 copyRegion{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = sizeof(EnvironmentGpuInfo)
        };

        vk::CopyBufferInfo2 copyInfo{
            .srcBuffer = *stagingBuffer,
            .dstBuffer = *environmentBuffer,
            .regionCount = 1,
            .pRegions = &copyRegion
        };

        commandBuffer.copyBuffer2(copyInfo);

        vk::BufferMemoryBarrier2 bufferBarrier{
            .srcStageMask =
                vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask =
                vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask =
                vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask =
                vk::AccessFlagBits2::eShaderRead,
            .srcQueueFamilyIndex =
                vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex =
                vk::QueueFamilyIgnored,
            .buffer = *environmentBuffer,
            .offset = 0,
            .size = sizeof(EnvironmentGpuInfo)
        };

        commandBuffer.pipelineBarrier2(
            vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers =
                    &bufferBarrier
            });

        if (auto result = commandBuffer.end();
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        if (auto result =
                submitAndWait(
                    context.device,
                    transferQueue,
                    commandBuffer);
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        resources.environmentBuffer =
            std::move(environmentBuffer);

        resources.skybox =
            std::move(skybox);

        resources.irradiance =
            std::move(irradiance);

        resources.radiance =
            std::move(radiance);

        std::cout
            << "[Environment] Environment resources created."
            << std::endl;

        return {
            vk::Result::eSuccess,
            std::move(resources)
        };
    }
}