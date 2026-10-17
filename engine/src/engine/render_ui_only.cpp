//
// Created by Shagu on 24.09.2026.
//
#include "engine_impl.hpp"

namespace shuttle::engine {
    vk::Result EngineImpl::renderUiOnly(
        IUiPainter &uiPainter,
        ImUiContextHandle imUiContextHandle,
        FrameResourcesHandle frameResourcesHandle,
        ViewportResourcesHandle viewportResourcesHandle,
        ViewportTexturesHandle viewportTexturesHandle,
        SwapchainResourcesHandle swapchainResourcesHandle
    ) {
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

        auto [imageIndexAcquireResult, imageIndex] = m_device.acquireNextImageKHR(
            *swapchain->swapchain,
            0,
            *frameResources->uniqueImageAvailableSemaphores[frameIndex],
            nullptr,
            m_dispatcher
        );
        if (imageIndexAcquireResult != vk::Result::eSuccess) return imageIndexAcquireResult;

        imUiContext->drawUi(uiPainter, frameIndex);

        auto cmd = *m_uniqueGraphicsCommandBuffers[frameIndex];

        if (auto cmdBeginResult = cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, m_dispatcher);
            cmdBeginResult != vk::Result::eSuccess) return cmdBeginResult;

        std::array toUiPassImageBarriers{
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
                    .layerCount = 1}
            }
        };

        cmd.pipelineBarrier2({
            .imageMemoryBarrierCount = static_cast<uint32_t>(toUiPassImageBarriers.size()),
            .pImageMemoryBarriers = toUiPassImageBarriers.data()}, m_dispatcher);

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

        vk::RenderingInfo const renderingInfo{
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

        if (auto cmdEndResult = cmd.end(m_dispatcher);
            cmdEndResult != vk::Result::eSuccess) return cmdEndResult;

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

        auto submitResult = m_graphicsQueue.submit2(submitInfo, fence, m_dispatcher);
        if (submitResult != vk::Result::eSuccess) return submitResult;

        m_swapchainResources.acquire(swapchainResourcesHandle);
        m_frameResources.acquire(frameResourcesHandle);
        m_imGuiContexts.acquire(imUiContextHandle);
        m_viewportResources.acquire(viewportResourcesHandle);
        imUiContext->acquireViewportTexture(viewportTexturesHandle);
        m_frameResources.get(frameResourcesHandle)->frameStatuses[frameIndex] = vk::Result::eNotReady;

        if (auto addWaitTaskResult = m_fenceWaiter.addWaitTask(fence,
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
        }); addWaitTaskResult != vk::Result::eSuccess) return addWaitTaskResult;

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

        return m_fenceWaiter.addWaitTask(fence1, [swapchainResourcesHandle, this](vk::Result) {
            m_swapchainResources.release(swapchainResourcesHandle);
        });
    }
}