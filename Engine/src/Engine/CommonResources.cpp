//
// Created by Shagu on 14.09.2026.
//
#include "tinyddsloader.h"
#include "EngineImpl.hpp"

namespace shuttle::engine {

    vk::Result EngineImpl::initFallbackTextures() {

        struct {
            std::array<std::byte, 4> albedo = std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
            std::array<std::byte, 4> normal = std::array{std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}};
            std::array<std::byte, 4> orm = std::array{std::byte{255}, std::byte{255}, std::byte{0}, std::byte{255}};
            std::array<std::byte, 4> emission = std::array{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255}};
        } fallbackValues;

        auto [createStagingBufferResult, stagingBuffer] = createStagingBuffer(sizeof(fallbackValues));
        if (createStagingBufferResult != vk::Result::eSuccess) return createStagingBufferResult;

        auto stagingBufferMappedPointer = m_allocator->getMappedPointer(*stagingBuffer);
        if (!stagingBufferMappedPointer) return vk::Result::eErrorMemoryMapFailed;

        std::memcpy(stagingBufferMappedPointer, &fallbackValues, sizeof(fallbackValues));

        // ============================================================
        // Albedo fallback
        // ============================================================

        auto [albedoTextureCreateResult, albedoTexture] = createTexture(
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageViewType::e2D,
            {.width = 1, .height = 1, .depth = 1},
            1,
            1
        );
        if (albedoTextureCreateResult != vk::Result::eSuccess) return albedoTextureCreateResult;

        m_fallbackTextures.indices.albedo = albedoTexture.descriptorSlot.get();
        m_fallbackTextures.storage.albedoTexture = std::move(albedoTexture);

        // ============================================================
        // Normal fallback
        // ============================================================

        auto [normalTextureCreateResult, normalTexture] = createTexture(
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageViewType::e2D,
            {.width = 1, .height = 1, .depth = 1},
            1,
            1
        );
        if (normalTextureCreateResult != vk::Result::eSuccess) return normalTextureCreateResult;

        m_fallbackTextures.indices.normal = normalTexture.descriptorSlot.get();
        m_fallbackTextures.storage.normalTexture = std::move(normalTexture);

        // ============================================================
        // ORM fallback
        // ============================================================

        auto [ormTextureCreateResult, ormTexture] = createTexture(
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageViewType::e2D,
            {.width = 1, .height = 1, .depth = 1},
            1,
            1
        );
        if (ormTextureCreateResult != vk::Result::eSuccess) return ormTextureCreateResult;

        m_fallbackTextures.indices.orm = ormTexture.descriptorSlot.get();
        m_fallbackTextures.storage.ormTexture = std::move(ormTexture);

        // ============================================================
        // Emission fallback
        // ============================================================

        auto [emissiveTextureCreateResult, emissiveTexture] = createTexture(
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageViewType::e2D,
            {.width = 1, .height = 1, .depth = 1},
            1,
            1
        );
        if (emissiveTextureCreateResult != vk::Result::eSuccess) return emissiveTextureCreateResult;

        m_fallbackTextures.indices.emission = emissiveTexture.descriptorSlot.get();
        m_fallbackTextures.storage.emissionTexture = std::move(emissiveTexture);

        auto [allocateCommandBufferResult, cmdBuffer] = allocateOneTimeCommandBuffer();
        if (allocateCommandBufferResult != vk::Result::eSuccess) return allocateCommandBufferResult;

        if (auto beginResult = cmdBuffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, m_dispatcher); beginResult != vk::Result::eSuccess) return beginResult;
        writeImageSourceBarrier(*cmdBuffer, {
            {.image = *albedoTexture.image, .mipLevels = 1},
            {.image = *normalTexture.image, .mipLevels = 1},
            {.image = *ormTexture.image, .mipLevels = 1},
            {.image = *emissiveTexture.image, .mipLevels = 1}
        });
        writeCopyBufferImageCommands(*cmdBuffer, *stagingBuffer,
            {*albedoTexture.image, *normalTexture.image, *ormTexture.image, *emissiveTexture.image}, {
            {.imageIndex = 0, .mipLevel = 0, .stagingOffset = offsetof(decltype(fallbackValues), albedo), .width = 1, .height = 1},
            {.imageIndex = 1, .mipLevel = 0, .stagingOffset = offsetof(decltype(fallbackValues), normal), .width = 1, .height = 1},
            {.imageIndex = 2, .mipLevel = 0, .stagingOffset = offsetof(decltype(fallbackValues), orm), .width = 1, .height = 1},
            {.imageIndex = 3, .mipLevel = 0, .stagingOffset = offsetof(decltype(fallbackValues), emission), .width = 1, .height = 1}}
        );
        writeBufferAndImageDestinationBarrier(*cmdBuffer, {
            {.image = *albedoTexture.image, .mipLevels = 1},
            {.image = *normalTexture.image, .mipLevels = 1},
            {.image = *ormTexture.image, .mipLevels = 1},
            {.image = *emissiveTexture.image, .mipLevels = 1}}, {}
        );
        if (auto endResult = cmdBuffer->end(m_dispatcher); endResult != vk::Result::eSuccess) return endResult;

        auto [createFenceResult, fence] = m_device.createFenceUnique({}, nullptr, m_dispatcher);
        if (createFenceResult != vk::Result::eSuccess) return createFenceResult;

        vk::CommandBufferSubmitInfo cmd{
            .commandBuffer = *cmdBuffer
        };

        if( auto submitResult = m_graphicsQueue.submit2(
            {{.commandBufferInfoCount = 1, .pCommandBufferInfos = &cmd}}, *fence, m_dispatcher
        ); submitResult != vk::Result::eSuccess) return submitResult;

        if (auto waitResult = m_device.waitForFences({*fence}, vk::False, UINT64_MAX, m_dispatcher); waitResult != vk::Result::eSuccess) return waitResult;

        return vk::Result::eSuccess;
    }

    vk::Result EngineImpl::submitRenderCommands(
        vk::CommandBuffer cmd,
        RenderIndices renderIndices,
        ImUiContextHandle const& imUiContextHandle,
        SwapchainResourcesHandle const& swapchainHandle,
        FrameResourcesHandle const& frameResourcesHandle,
        ViewportResourcesHandle const& viewportResourcesHandle,
        ViewportTexturesHandle viewportTexturesHandle) {

        auto swapchain = m_swapchainResources.get(swapchainHandle);
        auto frameResources = m_frameResources.get(frameResourcesHandle);
        auto imUiContext = m_imGuiContexts.get(imUiContextHandle);

        vk::SemaphoreSubmitInfo waitSemaphores{
            .semaphore = *frameResources->uniqueImageAvailableSemaphores[renderIndices.frameIndex],
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .deviceIndex = 0
        };
        vk::SemaphoreSubmitInfo signalSemaphores{
            .semaphore = *swapchain->renderFinishedSemaphores[renderIndices.imageIndex],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .deviceIndex = 0
        };
        vk::CommandBufferSubmitInfo commandBuffer{
            .commandBuffer = cmd,
            .deviceMask = 0x1
        };

        vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphores,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBuffer,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphores
        };

        auto [acquireFenceResult, fence] = m_fenceWaiter.acquireFence();
        if (acquireFenceResult != vk::Result::eSuccess) return acquireFenceResult;

        auto submitResult = m_graphicsQueue.submit2(submitInfo, fence, m_dispatcher);
        if (submitResult != vk::Result::eSuccess) return submitResult;

        m_swapchainResources.acquire(swapchainHandle);
        m_frameResources.acquire(frameResourcesHandle);
        m_imGuiContexts.acquire(imUiContextHandle);
        m_viewportResources.acquire(viewportResourcesHandle);
        imUiContext->acquireViewportTexture(viewportTexturesHandle);
        m_frameResources.get(frameResourcesHandle)->frameStatuses[renderIndices.frameIndex] = vk::Result::eNotReady;

        m_fenceWaiter.addWaitTask(fence,
        [
            swapchainHandle,
            frameResourcesHandle,
            imUiContextHandle,
            viewportResourcesHandle,
            viewportTexturesHandle,
            renderIndices,
            imUiContext,
            this] (
                vk::Result result)
        {
            m_swapchainResources.release(swapchainHandle);
            m_frameResources.release(frameResourcesHandle);
            m_imGuiContexts.release(imUiContextHandle);
            m_viewportResources.release(viewportResourcesHandle);
            imUiContext->releaseViewportTexture(viewportTexturesHandle);

            auto frameResourcesPtr = m_frameResources.get(frameResourcesHandle);
            if (frameResourcesPtr != nullptr) {
                frameResourcesPtr->frameStatuses[renderIndices.frameIndex] = result;
            }
        });

        return vk::Result::eSuccess;
    }

    vk::Result EngineImpl::presentSwapchain(SwapchainResourcesHandle const &swapchainHandle, uint32_t swapchainImageIndex) {

        auto swapchain = m_swapchainResources.get(swapchainHandle);

        auto [acquireFenceResult, fence] = m_fenceWaiter.acquireFence();
        if (acquireFenceResult != vk::Result::eSuccess) return acquireFenceResult;

        vk::Result presentResult{vk::Result::eSuccess};

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*swapchain->renderFinishedSemaphores[swapchainImageIndex],
            .swapchainCount = 1,
            .pSwapchains = &*swapchain->swapchain,
            .pImageIndices = &swapchainImageIndex,
            .pResults = &presentResult
        };

        auto result = m_graphicsQueue.presentKHR(presentInfo, m_dispatcher);
        if (result != vk::Result::eSuccess) return result;
        if (presentResult != vk::Result::eSuccess) return presentResult;

        m_swapchainResources.acquire(swapchainHandle);

        m_fenceWaiter.addWaitTask(fence, [&, this](vk::Result) {
            m_swapchainResources.release(swapchainHandle);
        });

        return vk::Result::eSuccess;
    }

    vk::ResultValue<render::CommonResources> EngineImpl::createCommonResources(std::filesystem::path const &brdfLutPath) {
        render::CommonResources common{};

        // ============================================================
        // Samplers
        // ============================================================

        auto [createMaterialSamplerResult, materialSampler] =
             m_device.createSamplerUnique(
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
                 .maxLod = vk::LodClampNone,
                 .borderColor = vk::BorderColor::eIntOpaqueBlack,
                 .unnormalizedCoordinates = vk::False
             });;

        if (createMaterialSamplerResult != vk::Result::eSuccess)
        {
            return {createMaterialSamplerResult, {}};
        }

        auto [createNearestSamplerResult, nearestSampler] =
            m_device.createSamplerUnique(
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
            });;

        if (createNearestSamplerResult != vk::Result::eSuccess)
        {
            return {createNearestSamplerResult, {}};
        }

        auto [createShadowSamplerResult, shadowSampler] =
            m_device.createSamplerUnique(
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
            });;

        if (createShadowSamplerResult != vk::Result::eSuccess)
        {
            return {createShadowSamplerResult, {}};
        }

        common.storage.materialSampler = std::move(materialSampler);
        common.storage.nearestSampler = std::move(nearestSampler);
        common.storage.shadowSampler = std::move(shadowSampler);

        // ============================================================
        // Write samplers into descriptor heap set
        // ============================================================

        auto [writeMaterialSamplerResult, materialSamplerIndex] =
            m_descriptorHeapSet.writeSamplerUnique(*common.storage.materialSampler);

        if (writeMaterialSamplerResult != vk::Result::eSuccess)
        {
            return {writeMaterialSamplerResult, {}};
        }

        common.info.materialSampler = materialSamplerIndex.get();
        common.storage.materialSamplerSlot = std::move(materialSamplerIndex);

        auto [writeNearestSamplerResult, nearestSamplerIndex] =
            m_descriptorHeapSet.writeSamplerUnique(*common.storage.nearestSampler);

        if (writeNearestSamplerResult != vk::Result::eSuccess)
        {
            return {writeNearestSamplerResult, {}};
        }

        common.info.nearestSampler = nearestSamplerIndex.get();
        common.storage.nearestSamplerSlot = std::move(nearestSamplerIndex);

        auto [writeShadowSamplerResult, shadowSamplerIndex] =
                m_descriptorHeapSet.writeSamplerUnique(*common.storage.shadowSampler);

        if (writeShadowSamplerResult != vk::Result::eSuccess)
        {
            return {writeShadowSamplerResult, {}};
        }

        common.info.shadowSampler = shadowSamplerIndex.get();
        common.storage.shadowSamplerSlot = std::move(shadowSamplerIndex);

        // ============================================================
        // BRDF LUT
        // ============================================================

        std::cout << "[CommonResources] Loading BRDF LUT: " << brdfLutPath.string() << std::endl;

        tinyddsloader::DDSFile ddsFile;

        const std::string pathString = brdfLutPath.string();

        if (const auto loadResult = ddsFile.Load(pathString.c_str()); loadResult != tinyddsloader::Result::Success)
        {
            std::cerr
                    << "[CommonResources] Failed to load BRDF LUT DDS."
                    << std::endl;

            return {vk::Result::eErrorInitializationFailed, {}};
        }

        assert(ddsFile.GetFormat() == tinyddsloader::DDSFile::DXGIFormat::R16G16_Float && "Expected DXGIFormat::R16G16_Float");

        auto imageData = ddsFile.GetImageData(0, 0);
        auto imageDataSize = imageData->m_memSlicePitch;
        auto imageDataWidth = imageData->m_width;
        auto imageDataHeight = imageData->m_height;
        auto imageDataPtr = imageData->m_mem;

        auto [createTextureResult, brdfTexture] = createTexture(
            vk::Format::eR16G16Sfloat,
            vk::ImageViewType::e2D,
            vk::Extent3D{ imageDataWidth, imageDataHeight, imageDataHeight },
            1, 1
        );
        if (createTextureResult != vk::Result::eSuccess) return {createTextureResult, {}};

        common.info.brdfLutTexture = brdfTexture.descriptorSlot.get();
        common.storage.brdfLutTexture = std::move(brdfTexture);

        // ============================================================
        // CommonResourcesInfo GPU Buffer
        // ============================================================

        auto [createInfoBufferResult, infoBuffer] = createShaderDeviceAddressBuffer(sizeof(render::CommonResourcesInfo));

        if (createInfoBufferResult != vk::Result::eSuccess)
        {
            return {createInfoBufferResult, {}};
        }

        common.infoBuffer = std::move(infoBuffer);
        common.infoAddress = m_device.getBufferAddress({ .buffer = *common.infoBuffer });

        vk::DeviceSize stagingTotalSize = sizeof(render::CommonResourcesInfo) + imageDataSize;
        auto [stagingBufferCreateResult, stagingBuffer] = createStagingBuffer(stagingTotalSize);
        if (stagingBufferCreateResult != vk::Result::eSuccess) return {stagingBufferCreateResult, {}};

        void* stagingBufferMappedPtr = m_allocator->getMappedPointer(*stagingBuffer);

        std::memcpy(stagingBufferMappedPtr, imageDataPtr, imageDataSize);
        std::memcpy(static_cast<char*>(stagingBufferMappedPtr) + imageDataSize, &common.info, imageDataSize);

        auto [commandBufferAllocateResult, cmd] = allocateOneTimeCommandBuffer();
        if (commandBufferAllocateResult != vk::Result::eSuccess) return {commandBufferAllocateResult, {}};

        if (auto cmdBeginResult = cmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, m_dispatcher); cmdBeginResult != vk::Result::eSuccess )
            return {cmdBeginResult, {}};

        writeImageSourceBarrier(*cmd, { {.image = *brdfTexture.image, .mipLevels = 0}});
        writeCopyBufferImageCommands(*cmd, *stagingBuffer,
              {*brdfTexture.image},
              {{.imageIndex = 0, .mipLevel = 0, .stagingOffset = 0, .width = imageDataWidth, .height = imageDataHeight, .dataSize = imageDataSize}}
        );
        writeCopyBufferCommand(*cmd, *stagingBuffer,
            {{.stagingOffset = imageDataSize, .destinationBuffer = *common.infoBuffer, .dataSize = sizeof(common.info)}}
        );
        writeBufferAndImageDestinationBarrier(*cmd,
            {{.image = *common.storage.brdfLutTexture.image, .mipLevels = 0}},
            {{.buffer = *common.infoBuffer, .size = sizeof(render::CommonResourcesInfo)}}
        );
        if (auto cmdEndResult = cmd->end(m_dispatcher); cmdEndResult != vk::Result::eSuccess)
            return {cmdEndResult, {}};

        return {
            submitTransferAndWait(*cmd),
            std::move(common)
        };
    }
}