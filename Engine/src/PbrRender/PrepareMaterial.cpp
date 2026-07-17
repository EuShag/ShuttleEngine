//
// Created by Shagu on 30.05.2026.
//
#include "Render.hpp"
#include "HostImageProcessor/HostImageProcessor.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine {

    void StagingBufferMaterialInfo::prepareCopyCommandsToBuffer(
        vk::CommandBuffer commandBuffer,
        vk::Buffer stagingBuffer,
        vk::Buffer propertiesUboBuffer, // Наш целевой UBO
        vk::Image dstAlbedoImage,
        vk::Image dstNormalImage,
        vk::Image dstOrmImage,
        vk::Image dstEmissionImage,
        vk::Image dstHeightImage
    ) const {

        // 1. Копируем блок свойств материала (Uniform Buffer)
        vk::BufferCopy uboCopyRegion{
            .srcOffset = propertiesOffset,
            .dstOffset = 0,
            .size = sizeof(HostMaterialProperties)
        };

        commandBuffer.copyBuffer(stagingBuffer, propertiesUboBuffer, 1, &uboCopyRegion);

        // Барьер для UBO: Transfer Write -> Uniform Read
        vk::BufferMemoryBarrier uboBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = {},
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .buffer = propertiesUboBuffer,
            .offset = 0,
            .size = sizeof(HostMaterialProperties)
        };

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            {}, 0, nullptr, 1, &uboBarrier, 0, nullptr
        );

        // 2. Вспомогательная лямбда для обработки текстур
        auto transferTexture = [&](vk::Image img, const StagingBufferImageInfo& info) {
            if (info.mipLevels.empty()) return;

            auto mips = static_cast<uint32_t>(info.mipLevels.size());

            // Барьер: Undefined -> TransferDst
            vk::ImageMemoryBarrier initialBarrier{
                .srcAccessMask = {},
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .image = img,
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mips, 0, 1 }
            };

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer,
                {}, 0, nullptr, 0, nullptr, 1, &initialBarrier
            );

            // Сбор регионов для всех мип-уровней
            std::vector<vk::BufferImageCopy> regions;
            regions.reserve(mips);
            for (uint32_t i = 0; i < mips; ++i) {
                regions.emplace_back(
                    info.stagingBufferOffset + info.mipLevels[i].offset,
                    0, 0,
                    vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, i, 0, 1 },
                    vk::Offset3D{ 0, 0, 0 },
                    vk::Extent3D{ info.mipLevels[i].width, info.mipLevels[i].height, 1 }
                );
            }

            commandBuffer.copyBufferToImage(stagingBuffer, img, vk::ImageLayout::eTransferDstOptimal, regions);
        };

        // 3. Выполняем трансфер для всех текстур материала
        transferTexture(dstAlbedoImage,   albedoInfo);
        transferTexture(dstNormalImage,   normalInfo);
        transferTexture(dstOrmImage,      ormInfo);
        transferTexture(dstEmissionImage, emissionInfo);
        transferTexture(dstHeightImage,   heightInfo);
    }

    void DeviceMaterialInfo::recordMaterialImagesBarriers(
        vk::CommandBuffer commandBuffer,
        uint32_t albedoMipLevelsCount,
        uint32_t normalMipLevelsCount,
        uint32_t ormMipLevelsCount,
        uint32_t emissionMipLevelsCount,
        uint32_t heightMipLevelsCount
    ) const {
        // Массив всех наших текстур для удобной итерации
        const std::array<std::pair<vk::Image, uint32_t>, 5> images = {
            std::pair{*albedoImage, albedoMipLevelsCount},
            std::pair{*normalImage, normalMipLevelsCount},
            std::pair{*ormImage, ormMipLevelsCount},
            std::pair{*emissionImage, emissionMipLevelsCount},
            std::pair{*heightImage, heightMipLevelsCount}
        };

        std::array<vk::ImageMemoryBarrier, 5> barriers = {};

        // Создаем барьер для каждой текстуры
        for (int i = 0; i < 5; ++i) {
            barriers[i] = {
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite, // После записи данных
                .dstAccessMask = vk::AccessFlagBits::eNone,    // Для чтения в шейдере
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = images[i].first,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = images[i].second,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };
        }

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            {},
            nullptr,
            nullptr,
            barriers
        );
    }

    PreparedHostMaterialData PbrRender::prepareHostMaterialData(HostMaterialData const &material) {
        PreparedHostMaterialData prepared;

        prepared.hostMaterialProperties = material.materialProperties;

        auto copyOrCreateEmpty = [](const std::optional<HostImageData>& optData,
                                    std::array<uint8_t, 4> defaultValue,
                                    vk::Format format,
                                    resources::MipFilter mipFilter)
        {
            if (optData.has_value()) {
                HostImageData hostData = optData.value();
                resources::TextureProcessor::prepareImageData(
                    hostData,
                    mipFilter);
                return hostData;
            }

            HostImageData emptyData;
            emptyData.width = 1;
            emptyData.height = 1;
            emptyData.imageFormat = format;

            // Просто копируем 4 байта напрямую в вектор!
            emptyData.data.assign(defaultValue.begin(), defaultValue.end());

            emptyData.mipChain.push_back({1, 1, 0, 4}); // Размер всегда 4 байта
            return emptyData;
        };

        prepared.albedoHostImageData = copyOrCreateEmpty(material.albedoTexture, PreparedHostMaterialData::defaultAlbedoValue, vk::Format::eR8G8B8A8Srgb, resources::MipFilter::Box);
        prepared.normalHostImageData = copyOrCreateEmpty(material.normalTexture, PreparedHostMaterialData::defaultNormalValue, vk::Format::eR8G8B8A8Unorm, resources::MipFilter::NormalMap);
        prepared.ormHostImageData = copyOrCreateEmpty(material.ormTexture, PreparedHostMaterialData::defaultOrmValue, vk::Format::eR8G8B8A8Unorm, resources::MipFilter::Box);
        prepared.emissionHostImageData = copyOrCreateEmpty(material.emissiveTexture, PreparedHostMaterialData::defaultEmissiveValue, vk::Format::eR8G8B8A8Srgb, resources::MipFilter::Box);
        prepared.heightHostImageData = copyOrCreateEmpty(material.heightTexture, PreparedHostMaterialData::defaultHeightValue, vk::Format::eR8G8B8A8Unorm, resources::MipFilter::Box);

        prepared.calculateOffsetsAndSize();

        return prepared;
    }

    vk::ResultValue<StagingBufferMaterialInfo> PbrRender::prepareStagingBufferMaterialInfo(
        PreparedHostMaterialData&& prepared,
        resources::DeviceAllocator const& allocator,
        resources::AllocatedBuffer const & stagingBuffer,
        vk::DeviceSize& stagingBufferOffset)
    {
        const vk::DeviceSize baseOffset = stagingBufferOffset;
        StagingBufferMaterialInfo info;

        // 1. Рассчитываем абсолютный офсет для Uniform-блока свойств
        info.propertiesOffset = baseOffset + prepared.stagingBufferOffsets.propertiesOffset;

        // 2. Заполняем навигатор для текстур (форматы теперь на месте, это супер!)
        auto fillImageInfo = [&](const HostImageData& hostImg, vk::DeviceSize localOffset) {
            StagingBufferImageInfo imgInfo;
            imgInfo.stagingBufferOffset = baseOffset + localOffset;
            imgInfo.imageSize   = vk::Extent2D{ .width = hostImg.width, .height = hostImg.height};
            imgInfo.imageFormat = hostImg.imageFormat;
            imgInfo.mipLevels   = hostImg.mipChain;
            return imgInfo;
        };

        info.albedoInfo   = fillImageInfo(prepared.albedoHostImageData,   prepared.stagingBufferOffsets.albedoOffset);
        info.normalInfo   = fillImageInfo(prepared.normalHostImageData,   prepared.stagingBufferOffsets.normalOffset);
        info.ormInfo      = fillImageInfo(prepared.ormHostImageData,      prepared.stagingBufferOffsets.ormOffset);
        info.emissionInfo = fillImageInfo(prepared.emissionHostImageData, prepared.stagingBufferOffsets.emissionOffset);
        info.heightInfo   = fillImageInfo(prepared.heightHostImageData,   prepared.stagingBufferOffsets.heightOffset);

        // --- ВОТ ЭТОТ БЛОК НУЖНО ДОБАВИТЬ ---
        // 3. Копируем свойства материала (UBO) в Staging Buffer
        if (auto result = allocator.writeBufferFromHost({
            .dstBuffer = stagingBuffer,
            .dstBufferOffset = info.propertiesOffset,
            .srcData = &prepared.hostMaterialProperties,
            .dataSize = sizeof(HostMaterialProperties),
        }); result != vk::Result::eSuccess) {
            return {result, {}};
        }
        // ------------------------------------

        // 4. Вспомогательная лямбда для копирования текстур
        auto copyToStaging = [&](const HostImageData& data, vk::DeviceSize absoluteOffset) -> vk::Result {
            if (data.data.empty()) return vk::Result::eSuccess;
            return allocator.writeBufferFromHost({
                .dstBuffer = stagingBuffer,
                .dstBufferOffset = absoluteOffset,
                .srcData = data.data.data(),
                .dataSize = data.data.size() * sizeof(data.data[0]),
            });
        };

        // 5. Копируем данные всех текстур
        if (auto result = copyToStaging(prepared.albedoHostImageData,   info.albedoInfo.stagingBufferOffset);   result != vk::Result::eSuccess) return {result, {}};
        if (auto result = copyToStaging(prepared.normalHostImageData,   info.normalInfo.stagingBufferOffset);   result != vk::Result::eSuccess) return {result, {}};
        if (auto result = copyToStaging(prepared.ormHostImageData,      info.ormInfo.stagingBufferOffset);      result != vk::Result::eSuccess) return {result, {}};
        if (auto result = copyToStaging(prepared.emissionHostImageData, info.emissionInfo.stagingBufferOffset); result != vk::Result::eSuccess) return {result, {}};
        if (auto result = copyToStaging(prepared.heightHostImageData,   info.heightInfo.stagingBufferOffset);   result != vk::Result::eSuccess) return {result, {}};

        // 6. Двигаем глобальный офсет с выравниванием (256 байт — стандарт для SSBO/UBO)
        stagingBufferOffset = alignUp(baseOffset + prepared.stagingBufferRequiredSize, vk::DeviceSize(256));

        return {vk::Result::eSuccess, info};
    }

    vk::ResultValue<DeviceMaterialInfo> PbrRender::prepareDeviceMaterialInfo(
        const StagingBufferMaterialInfo& stagingInfo,
        vk::Device device,
        resources::DeviceAllocator const& deviceAllocator) // Удалены commandBuffer и stagingBuffer
    {
        DeviceMaterialInfo material;

        // --- 1. Создаем Uniform Buffer для свойств материала ---
        vk::BufferCreateInfo uboInfo{
            .size = sizeof(HostMaterialProperties),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, // TransferDst нужен, т.к. будем копировать в него из staging
            .sharingMode = vk::SharingMode::eExclusive
        };

        auto uboRes = deviceAllocator.createAndAllocateBufferUnique(uboInfo, resources::MemoryUsage::eGpuOnly);
        if (uboRes.result != vk::Result::eSuccess) return {uboRes.result, {}};
        material.uniformBufferMaterialProperties = std::move(uboRes.value);

        // --- 2. Вспомогательная лямбда для создания Image и ImageView ---
        // Эта лямбда теперь НЕ занимается барьерами или копированием!
        auto createTextureResource = [&](
            const StagingBufferImageInfo& imgInfo,
            resources::UniqueAllocatedImage& outImage,
            vk::UniqueImageView& outView) -> vk::Result
        {
            if (imgInfo.mipLevels.empty()) return vk::Result::eSuccess; // Если нет мип-уровней, это "пустая" текстура

            // А. Создаем Image (usage: TransferDst для копирования, Sampled для чтения в шейдере)
            vk::ImageCreateInfo imageInfo{
                .imageType = vk::ImageType::e2D,
                .format = imgInfo.imageFormat,
                .extent = vk::Extent3D{imgInfo.imageSize.width, imgInfo.imageSize.height, 1},
                .mipLevels = static_cast<uint32_t>(imgInfo.mipLevels.size()),
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                .initialLayout = vk::ImageLayout::eUndefined // Начальный лейаут - Undefined
            };

            auto imgRes = deviceAllocator.createAndAllocateImageUnique(imageInfo, resources::MemoryUsage::eGpuOnly);
            if (imgRes.result != vk::Result::eSuccess) return imgRes.result;
            outImage = std::move(imgRes.value);

            // Б. Создаем ImageView
            vk::ImageViewCreateInfo viewInfo{
                .image = *outImage,
                .viewType = vk::ImageViewType::e2D,
                .format = imgInfo.imageFormat,
                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, imageInfo.mipLevels, 0, 1}
            };

            auto viewRes = device.createImageViewUnique(viewInfo);
            if (viewRes.result != vk::Result::eSuccess) return viewRes.result;
            outView = std::move(viewRes.value);

            return vk::Result::eSuccess;
        };

        // --- 3. Создаем ресурсы для всех 5 текстур ---
        if (auto res = createTextureResource(stagingInfo.albedoInfo,   material.albedoImage,   material.albedoTextureView);   res != vk::Result::eSuccess) return {res, {}};
        if (auto res = createTextureResource(stagingInfo.normalInfo,   material.normalImage,   material.normalTextureView);   res != vk::Result::eSuccess) return {res, {}};
        if (auto res = createTextureResource(stagingInfo.ormInfo,      material.ormImage,      material.ormTextureView);      res != vk::Result::eSuccess) return {res, {}};
        if (auto res = createTextureResource(stagingInfo.emissionInfo, material.emissionImage, material.emissionTextureView); res != vk::Result::eSuccess) return {res, {}};
        if (auto res = createTextureResource(stagingInfo.heightInfo,   material.heightImage,   material.heightTextureView);   res != vk::Result::eSuccess) return {res, {}};

        return {vk::Result::eSuccess, std::move(material)};
    }

    void PbrRender::fillDescriptorSet(
        vk::Device device,
        vk::DescriptorSet materialSet, // Теперь materialSet приходит извне
        const DeviceMaterialInfo& info)
    {
        // 1. Готовим инфу для записи в дескрипторы

        // Binding 0: Uniform Buffer (Свойства Материала)
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *info.uniformBufferMaterialProperties, // Разыменовываем UniqueAllocatedBuffer
            .offset = 0,
            .range = sizeof(HostMaterialProperties)
        };

        // Binding 1-5: Текстуры (Albedo, Normal, ORM, Emission, Height)
        // Важно: в DescriptorImageInfo imageLayout должен быть vk::ImageLayout::eShaderReadOnlyOptimal,
        // который мы устанавливаем барьером при копировании текстур.
        std::array<vk::DescriptorImageInfo, 5> texturesInfo{{
            { .sampler = nullptr, .imageView = *info.albedoTextureView,   .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = *info.normalTextureView,   .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = *info.ormTextureView,      .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = *info.emissionTextureView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = *info.heightTextureView,   .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal }
        }};

        // 2. Формируем массив write-операций
        std::array<vk::WriteDescriptorSet, 6> writeSets;

        // Write 0: Material Properties (UBO)
        writeSets[0] = vk::WriteDescriptorSet{
            .dstSet = materialSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo
        };

        // Write 1-5: Sampled Images (текстуры)
        for(int i = 1; i < 6; ++i) {
            writeSets[i] = vk::WriteDescriptorSet{
                .dstSet = materialSet,
                .dstBinding = static_cast<uint32_t>(i), // Биндинги 1, 2, 3, 4, 5
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = &texturesInfo[i - 1] // Image View передаем через pImageInfo
            };
        }

        // 3. Обновляем дескрипторный сет в памяти GPU
        device.updateDescriptorSets(writeSets, {});
    }
}