//
// Created by Shagu on 23.09.2026.
//
#include "engine_impl.hpp"
namespace shuttle::engine {

    namespace {
        struct alignas(16) WorldTransformUpdatePassData {
            vk::DeviceAddress worldTransformBufferAddress{};
            uint32_t firstNode{};
            uint32_t nodeCount{};

            uint32_t reserved0{};
            uint32_t reserved1{};
        };
        struct alignas(16) MeshInstancesCountPassData
        {
            vk::DeviceAddress indirectDrawCommandsBufferAddress{};
        };
        struct alignas(16) PrefixSumPassData
        {
            vk::DeviceAddress indirectDrawCommandsBufferAddress{};

            uint32_t padding0{};
            uint32_t padding1{};
        };
        struct alignas(16) InstanceRemapPassData
        {
            vk::DeviceAddress indirectDrawCommandsBufferAddress{};
            vk::DeviceAddress instanceRemapBufferAddress{};
            vk::DeviceAddress meshInstanceCursorBufferAddress{};

            uint32_t drawableCount{};
            uint32_t meshCount{};

            uint32_t reserved0{};
            uint32_t reserved1{};
            uint32_t reserved2{};
        };
        struct alignas(16) MainPassData {
            vk::DeviceAddress mainPassSettingsAddress;
            vk::DeviceAddress instanceRemapAddress;
            vk::DeviceAddress worldTransformBufferAddress;
            uint64_t reserved;
        };
    }

    vk::Result EngineImpl::renderSceneUi(
        bool isDebugMode,
        IUiPainter &uiPainter,
        render::MainPassSettings const& renderPassSettings,
        core::Camera const& camera,
        ImUiContextHandle imUiContextHandle,
        ViewportResourcesHandle viewportResourcesHandle,
        ViewportTexturesHandle viewportTexturesHandle,
        SwapchainResourcesHandle swapchainResourcesHandle,
        FrameResourcesHandle frameResourcesHandle,
        SceneHandle sceneHandle,
        EnvironmentHandle environmentHandle) {

        static constexpr uint32_t WorkGroupSize = 64u;

        auto viewportResources = m_viewportResources.get(viewportResourcesHandle);
        auto scene = m_uploadedScenes.get(sceneHandle);
        auto environment = m_environmentResources.get(environmentHandle);
        auto imUiContext = m_imGuiContexts.get(imUiContextHandle);
        auto const& swapchain = m_swapchainResources.get(swapchainResourcesHandle);
        auto const& frameResources = m_frameResources.get(frameResourcesHandle);

        bool frameReady = false;
        uint32_t frameIndex = 0;
        for (uint32_t i = 0; i <= frameResources->frameStatuses.size(); ++i) {
            if (frameResources->frameStatuses[i] == vk::Result::eSuccess) {
                frameIndex = i;
                frameReady = true;
            } else if (frameResources->frameStatuses[i] != vk::Result::eNotReady) {
                return frameResources->frameStatuses[i];
            }
        }
        if (!frameReady) return vk::Result::eNotReady;

        auto [imageIndexAcquireResult, imageIndex] = m_uniqueDevice->acquireNextImageKHR(
            *swapchain->swapchain,
            0,
            *frameResources->uniqueImageAvailableSemaphores[frameIndex],
            nullptr,
            m_dispatcher
        );
        if (imageIndexAcquireResult != vk::Result::eSuccess) return imageIndexAcquireResult;

        auto& worldTransformUpdatePipeline = *frameGraph.worldTransformUpdatePassPipeline;

        auto& cameraSystem = frameResources->m_cameraSystems[frameIndex];
        auto& mainPassSettings = frameResources->m_mainPassSettingSystems[frameIndex];

        cameraSystem.updateData(camera);
        mainPassSettings.updateSettings(renderPassSettings);

        render::RenderRootData renderRootData{
            .commonDataDeviceAddress = m_commonResources.infoAddress,
            .sceneDataDeviceAddress = m_uniqueDevice->getBufferAddress({.buffer = *scene->deviceSceneResources.sceneRootBuffer}, m_dispatcher),
            .environmentDataDeviceAddress = m_uniqueDevice->getBufferAddress({.buffer = *environment->environmentBuffer}, m_dispatcher),
            .cameraDataDeviceAddress = cameraSystem.getCameraDataAddress()};

        imUiContext->drawUi(uiPainter, frameIndex);

        auto cmd = *m_uniqueGraphicsCommandBuffers[frameIndex];

        if (auto cmdBeginResult = cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, m_dispatcher); cmdBeginResult != vk::Result::eSuccess)
            return cmdBeginResult;

        cmd.pushConstants(
            *m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment |
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(renderRootData),
            &renderRootData, m_dispatcher);

        auto heapDescriptorSet = m_descriptorHeapSet.getDescriptorSet();

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *m_pipelineLayout,
            0, 1,
            &heapDescriptorSet,
            0, nullptr, m_dispatcher);

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            *m_pipelineLayout,
            0, 1,
            &heapDescriptorSet,
            0, nullptr, m_dispatcher);

        cmd.bindIndexBuffer(
            *scene->deviceSceneResources.indexBuffer,
            0, vk::IndexType::eUint32,
            m_dispatcher);

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, worldTransformUpdatePipeline, m_dispatcher);

        for (size_t i = 0; i < scene->hostSceneData.levels.size(); ++i)
        {
            auto const& nodeLevelRange = scene->hostSceneData.levels[i];

            if (nodeLevelRange.count == 0) continue;

            WorldTransformUpdatePassData worldTransformUpdatePushConstants{
                .worldTransformBufferAddress = frameResources->m_worldTransformBufferAddresses[frameIndex],
                .firstNode = nodeLevelRange.startIndex,
                .nodeCount = nodeLevelRange.count,
                .reserved0 = 0,
                .reserved1 = 0
            };

            cmd.pushConstants(
                *m_pipelineLayout,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
                render::PassSpecificDataOffset,
                sizeof(worldTransformUpdatePushConstants),
                &worldTransformUpdatePushConstants, m_dispatcher);

            uint32_t const groupCount = (nodeLevelRange.count + WorkGroupSize - 1) / WorkGroupSize;

            cmd.dispatch(groupCount, 1, 1, m_dispatcher);

            // После последнего уровня барьер не нужен.
            if (i + 1 == scene->hostSceneData.levels.size()) continue;

            vk::BufferMemoryBarrier2 barrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
                .buffer = *frameResources->m_worldTransformBuffers[frameIndex],
                .offset = 0,
                .size = vk::WholeSize
            };

            cmd.pipelineBarrier2( vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &barrier
            }, m_dispatcher);
        }

        vk::BufferMemoryBarrier2 worldTransformToMeshInstanceCountBufferBarrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
            .srcAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
            .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .buffer = *frameResources->m_worldTransformBuffers[frameIndex],
            .offset = 0,
            .size = vk::WholeSize};

        cmd.pipelineBarrier2({
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &worldTransformToMeshInstanceCountBufferBarrier},
            m_dispatcher);

        if (scene->sceneFrameRequirements.meshCount == 0) return vk::Result::eIncomplete;

        const vk::DeviceSize indirectDrawCommandsBufferSize =
                static_cast<vk::DeviceSize>(scene->sceneFrameRequirements.meshCount) *
                sizeof(vk::DrawIndexedIndirectCommand);

        cmd.fillBuffer(
            *frameResources->m_indirectDrawCommandsBuffers[frameIndex],
            0,
            indirectDrawCommandsBufferSize,
            0u, m_dispatcher);

        vk::BufferMemoryBarrier2 clearToComputeBarrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask =
            vk::AccessFlagBits2::eShaderRead |
            vk::AccessFlagBits2::eShaderWrite,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .buffer = *frameResources->m_indirectDrawCommandsBuffers[frameIndex],
            .offset = 0,
            .size = indirectDrawCommandsBufferSize
        };

        cmd.pipelineBarrier2(
            vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &clearToComputeBarrier
            }, m_dispatcher);


        if (scene->sceneFrameRequirements.drawableObjectCount == 0) return vk::Result::eIncomplete;

        MeshInstancesCountPassData meshInstanceCountPassPushConstants{
            .indirectDrawCommandsBufferAddress = frameResources->m_worldTransformBufferAddresses[frameIndex]
        };

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *frameGraph.meshInstanceCountPassPipeline,
            m_dispatcher);

        cmd.pushConstants(
            *m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            render::PassSpecificDataOffset,
            sizeof(meshInstanceCountPassPushConstants),
            &meshInstanceCountPassPushConstants, m_dispatcher);

        const uint32_t groupCount = (scene->sceneFrameRequirements.drawableObjectCount + WorkGroupSize - 1u) / WorkGroupSize;

        cmd.dispatch(
            groupCount, 1, 1,
            m_dispatcher);

        std::array countToPrefixSumBufferBarriers{
            vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
                .buffer = *frameResources->m_indirectDrawCommandsBuffers[frameIndex],
                .offset = 0,
                .size = vk::WholeSize}};

        cmd.pipelineBarrier2({
            .bufferMemoryBarrierCount = static_cast<uint32_t>(countToPrefixSumBufferBarriers.size()),
            .pBufferMemoryBarriers = countToPrefixSumBufferBarriers.data()},
            m_dispatcher);

        PrefixSumPassData prefixSumPushConstants{
            .indirectDrawCommandsBufferAddress = frameResources->m_indirectDrawCommandsBufferAddresses[frameIndex],
            .padding0 = 0,
            .padding1 = 0
        };

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *frameGraph.prefixSumPassPipeline,
            m_dispatcher);

        cmd.pushConstants(
            *m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            render::PassSpecificDataOffset,
            sizeof(PrefixSumPassData),
            &prefixSumPushConstants,
            m_dispatcher);

        cmd.dispatch(
            1, 1, 1,
            m_dispatcher);

        if (scene->sceneFrameRequirements.drawableObjectCount == 0 ||
            scene->sceneFrameRequirements.meshCount == 0) return vk::Result::eIncomplete;

        const vk::DeviceSize meshInstanceCursorBufferSize =
                static_cast<vk::DeviceSize>(scene->sceneFrameRequirements.meshCount) *
                sizeof(uint32_t);

        std::array prefixSumToInstanceRemapBufferBarriers{
            vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .buffer = *frameResources->m_indirectDrawCommandsBuffers[frameIndex],
                .offset = 0,
                .size = vk::WholeSize}};

        cmd.pipelineBarrier2({
            .bufferMemoryBarrierCount = static_cast<uint32_t>(prefixSumToInstanceRemapBufferBarriers.size()),
            .pBufferMemoryBarriers = prefixSumToInstanceRemapBufferBarriers.data()},
            m_dispatcher);

        cmd.fillBuffer(
            *frameResources->m_meshInstanceCursorBuffers[frameIndex],
            0, meshInstanceCursorBufferSize,
            0u, m_dispatcher);

        vk::BufferMemoryBarrier2 clearToComputeBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask =
            vk::AccessFlagBits2::eShaderRead |
            vk::AccessFlagBits2::eShaderWrite,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .buffer = *frameResources->m_meshInstanceCursorBuffers[frameIndex],
            .offset = 0,
            .size = meshInstanceCursorBufferSize
        };

        cmd.pipelineBarrier2(
            vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &clearToComputeBarrier2
            }, m_dispatcher);


        InstanceRemapPassData InstanceRemapPushConstants{
            .indirectDrawCommandsBufferAddress = frameResources->m_indirectDrawCommandsBufferAddresses[frameIndex],
            .instanceRemapBufferAddress = frameResources->m_instanceRemapBufferAddresses[frameIndex],
            .meshInstanceCursorBufferAddress = frameResources->m_meshInstanceCursorBufferAddresses[frameIndex],
            .drawableCount = scene->sceneFrameRequirements.drawableObjectCount,
            .meshCount = scene->sceneFrameRequirements.meshCount,

            .reserved0 = 0,
            .reserved1 = 0,
            .reserved2 = 0
        };

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *frameGraph.instanceRemapPassPipeline,
            m_dispatcher);

        cmd.pushConstants(
            *m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            render::PassSpecificDataOffset,
            sizeof(InstanceRemapPassData),
            &InstanceRemapPushConstants,
            m_dispatcher);

        const uint32_t groupCount1 =
                (scene->sceneFrameRequirements.drawableObjectCount + WorkGroupSize - 1u) /
                WorkGroupSize;

        cmd.dispatch(
            groupCount1, 1, 1,
            m_dispatcher);


        std::array toMainRenderPassBufferMemoryBarriers{
            vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eVertexShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .buffer = *frameResources->m_instanceRemapBuffers[frameIndex],
                .offset = 0,
                .size = vk::WholeSize},
            vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
                .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                .buffer = *frameResources->m_indirectDrawCommandsBuffers[frameIndex],
                .offset = 0,
                .size = vk::WholeSize},
            vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
                .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                .buffer = *frameResources->m_worldTransformBuffers[frameIndex],
                .offset = 0,
                .size = vk::WholeSize}};
        std::array toMainRenderPassImageMemoryBarriers{
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_colorAttachmentOutputs[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_depthAttachmentOutputs[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eDepth,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs1[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs2[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs3[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs4[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}}};

        cmd.pipelineBarrier2({
            .bufferMemoryBarrierCount = static_cast<uint32_t>(toMainRenderPassBufferMemoryBarriers.size()),
            .pBufferMemoryBarriers = toMainRenderPassBufferMemoryBarriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(toMainRenderPassImageMemoryBarriers.size()),
            .pImageMemoryBarriers = toMainRenderPassImageMemoryBarriers.data()},
            m_dispatcher);

        auto const currentMainPipeline = isDebugMode ? *frameGraph.mainPassDebugPipeline : *frameGraph.mainPassPipeline;
        auto const currentSkyboxPipeline = isDebugMode ? *frameGraph.skyboxDebugPipeline : *frameGraph.skyboxPipeline;

        std::array const colorAttachments {
            vk::RenderingAttachmentInfo {
                .imageView = *viewportResources->m_colorAttachmentOutputs[frameIndex].view,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{.color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 0.0f}}}
            },
            vk::RenderingAttachmentInfo {
                .imageView = *viewportResources->m_debugOutputs1[frameIndex].view,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 0.0f}}
                }
            },
            vk::RenderingAttachmentInfo {
                .imageView = *viewportResources->m_debugOutputs2[frameIndex].view,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 0.0f}}
                }
            },
            vk::RenderingAttachmentInfo {
                .imageView = *viewportResources->m_debugOutputs3[frameIndex].view,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 0.0f}}
                }
            },
            vk::RenderingAttachmentInfo {
                .imageView = *viewportResources->m_debugOutputs4[frameIndex].view,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 0.0f}}
                }
            }
        };

        vk::RenderingAttachmentInfo const depthAttachment {
            .imageView = *viewportResources->m_depthAttachmentOutputs[frameIndex].view,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .resolveMode = vk::ResolveModeFlagBits::eNone,
            .resolveImageView = nullptr,
            .resolveImageLayout = vk::ImageLayout::eUndefined,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0}}
        };

        MainPassData mainPassData{
            .mainPassSettingsAddress = frameResources->m_mainPassSettingSystems[frameIndex].getMainPassSettingsBufferAddress(),
            .instanceRemapAddress = frameResources->m_instanceRemapBufferAddresses[frameIndex],
            .worldTransformBufferAddress = frameResources->m_worldTransformBufferAddresses[frameIndex],
            .reserved = 0,
        };

        cmd.pushConstants(
            *m_pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            render::PassSpecificDataOffset, sizeof(MainPassData),
            &mainPassData, m_dispatcher
        );

        auto viewportExtent = viewportResources->extent;

        cmd.beginRendering(
            vk::RenderingInfo{
                .renderArea = vk::Rect2D{
                    .offset = vk::Offset2D{.x = 0, .y = 0},
                    .extent = viewportExtent
                },
                .layerCount = 1,
                .colorAttachmentCount = isDebugMode ? static_cast<uint32_t>(colorAttachments.size()) : 1,
                .pColorAttachments = colorAttachments.data(),
                .pDepthAttachment = &depthAttachment
            }, m_dispatcher
        );

        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(viewportExtent.width),
            .height = static_cast<float>(viewportExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vk::Rect2D const scissor{
            .offset = vk::Offset2D{.x = 0, .y = 0},
            .extent = viewportExtent
        };

        cmd.setViewport(0, 1, &viewport, m_dispatcher);
        cmd.setScissor(0, 1, &scissor, m_dispatcher);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            currentMainPipeline, m_dispatcher);

        cmd.drawIndexedIndirect(
            *frameResources->m_indirectDrawCommandsBuffers[frameIndex],
            0,
            scene->sceneFrameRequirements.meshCount,
            sizeof(vk::DrawIndexedIndirectCommand),
            m_dispatcher);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            currentSkyboxPipeline,
            m_dispatcher);

        cmd.draw(36, 1, 0, 0, m_dispatcher);

        cmd.endRendering(m_dispatcher);

        std::array mainRenderPassToPresentImageBarriers{
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = swapchain->images[imageIndex],
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_colorAttachmentOutputs[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs1[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs2[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs3[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *viewportResources->m_debugOutputs4[frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}}};

        auto barrierCount = static_cast<uint32_t>(mainRenderPassToPresentImageBarriers.size());

        cmd.pipelineBarrier2({
            .imageMemoryBarrierCount = barrierCount,
            .pImageMemoryBarriers = mainRenderPassToPresentImageBarriers.data()},
            m_dispatcher);

        vk::RenderingAttachmentInfo swapchainAttachmentInfo{
            .imageView = *swapchain->imageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode = vk::ResolveModeFlagBits::eNone,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = vk::ImageLayout::eUndefined,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = { .color {std::array{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        vk::RenderingInfo renderingInfo{
            .renderArea = {
                .offset = {.x = 0, .y = 0},
                .extent = swapchain->extent
            },
            .layerCount = 1,
            .viewMask = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &swapchainAttachmentInfo
        };

        cmd.beginRendering(&renderingInfo, m_dispatcher);
        imUiContext->writeRenderCommands(cmd);
        cmd.endRendering(m_dispatcher);

        std::array uiPassToPresentImageBarriers{
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eNone,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::ePresentSrcKHR,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = swapchain->images[imageIndex],
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}}};

        cmd.pipelineBarrier2({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = uiPassToPresentImageBarriers.data()},
            m_dispatcher);

        if (auto cmdEndResult = cmd.end(m_dispatcher); cmdEndResult != vk::Result::eSuccess) {
            return cmdEndResult;
        }

        vk::SemaphoreSubmitInfo waitSemaphores{
            .semaphore = *frameResources->uniqueImageAvailableSemaphores[frameIndex],
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .deviceIndex = 0
        };
        vk::SemaphoreSubmitInfo signalSemaphores{
            .semaphore = *swapchain->renderFinishedSemaphores[imageIndex],
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

        if (auto submitResult = m_graphicsQueue.submit2(submitInfo, fence, m_dispatcher); submitResult != vk::Result::eSuccess) return submitResult;

        m_swapchainResources.acquire(swapchainResourcesHandle);
        m_frameResources.acquire(frameResourcesHandle);
        m_imGuiContexts.acquire(imUiContextHandle);
        m_viewportResources.acquire(viewportResourcesHandle);
        imUiContext->acquireViewportTexture(viewportTexturesHandle);
        m_frameResources.get(frameResourcesHandle)->frameStatuses[frameIndex] = vk::Result::eNotReady;

        if (auto addTaskResult = m_fenceWaiter.addWaitTask(fence,
        [
            swapchainResourcesHandle,
            frameResourcesHandle,
            imUiContextHandle,
            viewportResourcesHandle,
            viewportTexturesHandle,
            frameIndex,
            imUiContext,
            this] (
                vk::Result result)
        {
            m_swapchainResources.release(swapchainResourcesHandle);
            m_frameResources.release(frameResourcesHandle);
            m_imGuiContexts.release(imUiContextHandle);
            m_viewportResources.release(viewportResourcesHandle);
            imUiContext->releaseViewportTexture(viewportTexturesHandle);

            auto frameResourcesPtr = m_frameResources.get(frameResourcesHandle);
            if (frameResourcesPtr != nullptr) {
                frameResourcesPtr->frameStatuses[frameIndex] = result;
            }
        }); addTaskResult != vk::Result::eSuccess) return addTaskResult;

        auto [acquireFenceResult1, fence1] = m_fenceWaiter.acquireFence();
        if (acquireFenceResult1 != vk::Result::eSuccess) return acquireFenceResult1;

        vk::SwapchainPresentFenceInfoKHR swapchainPresentFenceInfo{
            .swapchainCount = 1,
            .pFences = &fence1
        };

        vk::PresentInfoKHR presentInfo{
            .pNext = &swapchainPresentFenceInfo,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*swapchain->renderFinishedSemaphores[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &*swapchain->swapchain,
            .pImageIndices = &imageIndex
        };

        if (auto result = m_graphicsQueue.presentKHR(presentInfo, m_dispatcher); result != vk::Result::eSuccess) return result;

        m_swapchainResources.acquire(swapchainResourcesHandle);

        return m_fenceWaiter.addWaitTask(fence1, [&, this](vk::Result) {
            m_swapchainResources.release(swapchainResourcesHandle);
        });
    }
}