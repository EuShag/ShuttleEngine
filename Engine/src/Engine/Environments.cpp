//
// Created by Shagu on 18.09.2026.
//

#include "EngineImpl.hpp"
#include "Render/SceneData.hpp"

namespace shuttle::engine {

    namespace {

        void* getDestinationAddress(void* ptr, vk::DeviceSize offset = 0)
        {
            return static_cast<char*>(ptr) + offset;
        }

        vk::ImageType toVkImageType(
            assets::formats::texture::ImageType type)
        {
            using T = assets::formats::texture::ImageType;

            switch (type) {

                case T::Image1D: return vk::ImageType::e1D;
                case T::Image2D: return vk::ImageType::e2D;
                case T::Image3D: return vk::ImageType::e3D;
                default: return vk::ImageType::e2D;
            }
        }

        vk::ImageViewType toVkImageViewType(
            assets::formats::texture::ImageViewType type)
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
    }

    vk::ResultValue<EnvironmentHandle> EngineImpl::createEnvironmentResources(
            render::LoadedEnvironmentData const &data) {

        auto const& environmentInfo = *data.environmentInfos.begin();

        auto const& skyboxImageMetadata = data.textureMetadatas[environmentInfo.skyboxTextureIndex];
        auto const& irradianceImageMetadata = data.textureMetadatas[environmentInfo.irradianceTextureIndex];
        auto const& radianceImageMetadata = data.textureMetadatas[environmentInfo.prefilteredTextureIndex];

        auto const& rawImageData = data.textureBytes;

        auto const& skyboxImageMipMetadatas = data.textureMipMetadatas.subspan(skyboxImageMetadata.mipTableOffset, skyboxImageMetadata.mipCount);
        auto const& irradianceImageMipMetadatas = data.textureMipMetadatas.subspan(irradianceImageMetadata.mipTableOffset, irradianceImageMetadata.mipCount);
        auto const& radianceImageMipMetadatas = data.textureMipMetadatas.subspan(radianceImageMetadata.mipTableOffset, radianceImageMetadata.mipCount);

        auto skyboxImageType = toVkImageType(skyboxImageMetadata.imageType);
        auto skyboxImageViewType = toVkImageViewType(skyboxImageMetadata.imageViewType);
        auto irradianceImageType = toVkImageType(irradianceImageMetadata.imageType);
        auto irradianceImageViewType = toVkImageViewType(irradianceImageMetadata.imageViewType);
        auto radianceImageType = toVkImageType(radianceImageMetadata.imageType);
        auto radianceImageViewType = toVkImageViewType(radianceImageMetadata.imageViewType);

        assert(skyboxImageType == vk::ImageType::e2D && "Image Type must be 2D");
        assert(skyboxImageViewType == vk::ImageViewType::eCube && "Image View Type must be Cube");

        assert(irradianceImageType == vk::ImageType::e2D && "Image Type must be 2D");
        assert(irradianceImageViewType == vk::ImageViewType::eCube && "Image View Type must be Cube");

        assert(radianceImageType == vk::ImageType::e2D && "Image Type must be 2D");
        assert(radianceImageViewType == vk::ImageViewType::eCube && "Image View Type must be Cube");

        auto [createSkyboxTextureResult, skyboxTexture] = createTexture(
            static_cast<vk::Format>(skyboxImageMetadata.format),
            skyboxImageViewType,
            {.width = skyboxImageMetadata.width, .height = skyboxImageMetadata.height, .depth = skyboxImageMetadata.depth},
            skyboxImageMetadata.mipCount,
            skyboxImageMetadata.layerCount
        );
        if (createSkyboxTextureResult != vk::Result::eSuccess) {
            return {createSkyboxTextureResult, {}};
        }
        auto [createIrradianceTextureResult, irradianceTexture] = createTexture(
            static_cast<vk::Format>(irradianceImageMetadata.format),
            irradianceImageViewType,
            {.width = irradianceImageMetadata.width, .height = irradianceImageMetadata.height, .depth = irradianceImageMetadata.depth},
            irradianceImageMetadata.mipCount,
            irradianceImageMetadata.layerCount
        );
        if (createIrradianceTextureResult != vk::Result::eSuccess) {
            return {createIrradianceTextureResult, {}};
        }
        auto [createRadianceTextureResult, radianceTexture] = createTexture(
            static_cast<vk::Format>(radianceImageMetadata.format),
            radianceImageViewType,
            {.width = radianceImageMetadata.width, .height = radianceImageMetadata.height, .depth = radianceImageMetadata.depth},
            radianceImageMetadata.mipCount,
            radianceImageMetadata.layerCount
        );
        if (createRadianceTextureResult != vk::Result::eSuccess) return {createRadianceTextureResult, {}};

        render::EnvironmentGpuInfo gpuInfo{
            .skyboxTexture = skyboxTexture.descriptorSlot.get(),
            .irradianceTexture = irradianceTexture.descriptorSlot.get(),
            .radianceTexture = radianceTexture.descriptorSlot.get(),
            .radianceMipLevels = radianceImageMetadata.mipCount
        };

        auto [gpuInfoBufferCreateResult, gpuInfoBuffer] = createShaderDeviceAddressBuffer(sizeof(gpuInfo));
        if (gpuInfoBufferCreateResult != vk::Result::eSuccess) return {gpuInfoBufferCreateResult, {}};

        auto alignUp = [](vk::DeviceSize value, vk::DeviceSize alignment)
        { return value + alignment - 1 & ~(alignment - 1); };

        EnvironmentUploadOffsets uploadOffsets;

        uploadOffsets.environmentInfo = 0;

        constexpr vk::DeviceSize Alignment = 16;
        vk::DeviceSize stagingCursor = sizeof(gpuInfo);
        stagingCursor = alignUp(stagingCursor, Alignment);


        for (auto i = 0U; i < skyboxImageMipMetadatas.size(); ++i) {
            const auto& skyboxMip = skyboxImageMipMetadatas[i];
            uploadOffsets.skyboxMips.push_back(
                {
                    .imageIndex = 0,
                    .mipLevel = i,
                    .sourceOffset = skyboxMip.dataOffset,
                    .stagingOffset = stagingCursor,
                    .width = skyboxMip.width,
                    .height = skyboxMip.height,
                    .dataSize = skyboxMip.dataSize
                }
            );

            stagingCursor += skyboxMip.dataSize;
            stagingCursor = alignUp(stagingCursor, Alignment);
        }

        for (auto i = 0U; i < irradianceImageMipMetadatas.size(); ++i) {
            const auto& irradianceMip = irradianceImageMipMetadatas[i];
            uploadOffsets.irradianceMips.push_back(
                {
                    .imageIndex = 0,
                    .mipLevel = i,
                    .sourceOffset = irradianceMip.dataOffset,
                    .stagingOffset = stagingCursor,
                    .width = irradianceMip.width,
                    .height = irradianceMip.height,
                    .dataSize = irradianceMip.dataSize
                }
            );

            stagingCursor += irradianceMip.dataSize;
            stagingCursor = alignUp(stagingCursor, Alignment);
        }

        for (auto i = 0U; i < radianceImageMipMetadatas.size(); ++i) {
            const auto& radianceMip = radianceImageMipMetadatas[i];
            uploadOffsets.prefilterMips.push_back(
                {
                    .imageIndex = 0,
                    .mipLevel = i,
                    .sourceOffset = radianceMip.dataOffset,
                    .stagingOffset = stagingCursor,
                    .width = radianceMip.width,
                    .height = radianceMip.height,
                    .dataSize = radianceMip.dataSize
                }
            );

            stagingCursor += radianceMip.dataSize;
            stagingCursor = alignUp(stagingCursor, Alignment);
        }

        uploadOffsets.totalSize = stagingCursor;

        auto [stagingBufferCreateResult, stagingBuffer] = createStagingBuffer(uploadOffsets.totalSize);
        if (stagingBufferCreateResult != vk::Result::eSuccess) return {stagingBufferCreateResult, {}};

        void* stagingBufferMappedPtr = m_allocator->getMappedPointer(*stagingBuffer);
        if (stagingBufferMappedPtr == nullptr) return {vk::Result::eErrorMemoryMapFailed, {}};

        auto copyToStaging = [stagingBufferMappedPtr](vk::DeviceSize dstOffset, const void* src, size_t size)
        {
            if (size == 0) return;
            std::memcpy(getDestinationAddress(stagingBufferMappedPtr, dstOffset), src, size);
        };

        copyToStaging(uploadOffsets.environmentInfo, &gpuInfo, sizeof(gpuInfo));

        for (auto const& mip: uploadOffsets.skyboxMips) {
            std::memcpy(
                getDestinationAddress(stagingBufferMappedPtr, mip.stagingOffset),
                rawImageData.data() + mip.sourceOffset,
                mip.dataSize
            );
        }

        for (auto const& mip : uploadOffsets.irradianceMips) {
            std::memcpy(
                getDestinationAddress(stagingBufferMappedPtr, mip.stagingOffset),
                rawImageData.data() + mip.sourceOffset,
                mip.dataSize
            );
        }

        for (auto const& mip : uploadOffsets.prefilterMips) {
            std::memcpy(
                getDestinationAddress(stagingBufferMappedPtr, mip.stagingOffset),
                rawImageData.data() + mip.sourceOffset,
                mip.dataSize
            );
        }

        auto [commandBufferAllocateResult, cmd] = allocateOneTimeCommandBuffer();
        if (commandBufferAllocateResult != vk::Result::eSuccess) return {commandBufferAllocateResult, {}};

        if (auto cmdBeginResult = cmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, m_dispatcher); cmdBeginResult != vk::Result::eSuccess) {
            return {cmdBeginResult, {}};
        }

        writeImageSourceBarrier(*cmd,
            {{.image = *skyboxTexture.image, .mipLevels = skyboxImageMetadata.mipCount},
            {.image = *irradianceTexture.image, .mipLevels = irradianceImageMetadata.mipCount},
            {.image = *radianceTexture.image, .mipLevels = radianceImageMetadata.mipCount}}
        );
        writeCopyBufferImageCommands(*cmd, *stagingBuffer,
            {*skyboxTexture.image},
            uploadOffsets.skyboxMips
        );
        writeCopyBufferImageCommands(*cmd, *stagingBuffer,
            {*irradianceTexture.image},
            uploadOffsets.irradianceMips
        );
        writeCopyBufferImageCommands(*cmd, *stagingBuffer,
            {*radianceTexture.image},
            uploadOffsets.prefilterMips
        );
        writeCopyBufferCommand(*cmd, *stagingBuffer,
            {{.stagingOffset = uploadOffsets.environmentInfo, .destinationBuffer = *gpuInfoBuffer, .dataSize = sizeof(gpuInfo)}}
        );
        writeBufferAndImageDestinationBarrier(*cmd,
            {{.image = *skyboxTexture.image, .mipLevels = skyboxImageMetadata.mipCount},
            {.image = *irradianceTexture.image, .mipLevels = irradianceImageMetadata.mipCount},
            {.image = *radianceTexture.image, .mipLevels = radianceImageMetadata.mipCount}},
            {{.buffer = *gpuInfoBuffer, .size = sizeof(gpuInfo)}}
        );
        if (auto cmdEndResult = cmd->end(m_dispatcher); cmdEndResult != vk::Result::eSuccess) {
            return {cmdEndResult, {}};
        }
        if (auto submitAndWaitResult = submitTransferAndWait(*cmd); submitAndWaitResult != vk::Result::eSuccess) return {submitAndWaitResult, {}};

        render::DeviceEnvironmentResources resources{
            .environmentBuffer = std::move(gpuInfoBuffer),
            .skybox = std::move(skyboxTexture),
            .irradiance = std::move(irradianceTexture),
            .radiance = std::move(radianceTexture),
        };

        auto handle = m_environmentResources.allocate(std::move(resources));
        return {vk::Result::eSuccess, handle};
    }
}
