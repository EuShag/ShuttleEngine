//
// Created by Shagu on 15.06.2026.
//

#include "FrameManager.hpp"
#include <iostream>

namespace shuttle
{

vk::ResultValue<FrameManager>
FrameManager::create(vk::Device device,
                     uint32_t framesInFlightCount, // Чтобы определить количества семафоров для ожидания рендеринга и
                                                   // фенсов для командного буфера
                     uint32_t swapchainImageCount, FrameManager&& oldFrameManager)
{
    FrameManager result{framesInFlightCount, swapchainImageCount};
    if (auto initResult = result.init(device, std::move(oldFrameManager)); initResult != vk::Result::eSuccess)
    {
        return {initResult, {}};
    }
    return {vk::Result::eSuccess, std::move(result)};
}

vk::Result FrameManager::prepareFrameSlot(vk::Device device, uint32_t frameIndex)
{
    return device.waitForFences({*uniqueInFlightFences[frameIndex]}, true, UINT64_MAX);
}

vk::Result FrameManager::beginFrame(vk::Device device, uint32_t frameIndex) {
    return device.resetFences({*uniqueInFlightFences[frameIndex]});
}

void FrameManager::addOldDepthAttachment(resources::UniqueAllocatedImage&& oldDepthImage,
                                         vk::UniqueImageView&& oldDepthImageView) {
    oldDepthImages.push_back(std::move(oldDepthImage));
    oldDepthImageViews.push_back(std::move(oldDepthImageView));
}

vk::ResultValue<uint32_t> FrameManager::acquireNextImage(vk::Device device, vk::SwapchainKHR swapchain, uint32_t frameIndex)
{
    return device.acquireNextImageKHR(swapchain, UINT64_MAX, *uniqueImageAvailableSemaphores[frameIndex]);

}

vk::Result FrameManager::submitRenderCommands(vk::Queue graphicsQueue, vk::CommandBuffer cmd, uint32_t frameIndex,
                                              uint32_t imageIndex)
{
    constexpr vk::PipelineStageFlags2 waitPipelineStagesMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    constexpr vk::PipelineStageFlags2 signalPipelineStagesMask = vk::PipelineStageFlagBits2::eAllCommands;
    vk::SemaphoreSubmitInfo waitSemaphoreInfo{.semaphore = *uniqueImageAvailableSemaphores[frameIndex],
                                              .stageMask = waitPipelineStagesMask};
    vk::SemaphoreSubmitInfo signalSemaphoreInfo{.semaphore = *uniqueRenderFinishedSemaphores[imageIndex],
                                                .stageMask = signalPipelineStagesMask};
    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{.commandBuffer = cmd, .deviceMask = 1};

    return graphicsQueue.submit2({{.waitSemaphoreInfoCount = 1,
                                   .pWaitSemaphoreInfos = &waitSemaphoreInfo,
                                   .commandBufferInfoCount = 1,
                                   .pCommandBufferInfos = &commandBufferSubmitInfo,
                                   .signalSemaphoreInfoCount = 1,
                                   .pSignalSemaphoreInfos = &signalSemaphoreInfo}},
                                 *uniqueInFlightFences[frameIndex]);
}

vk::Result FrameManager::present(vk::Queue presentQueue, vk::SwapchainKHR swapchain, uint32_t imageIndex)
{
    return presentQueue.presentKHR({.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &*uniqueRenderFinishedSemaphores[imageIndex],
                                    .swapchainCount = 1,
                                    .pSwapchains = &swapchain,
                                    .pImageIndices = &imageIndex});
}

vk::Result FrameManager::waitRenderIdle(vk::Device device) const noexcept {
    std::vector<vk::Fence> fences;
    fences.reserve(uniqueInFlightFences.size());
    for (const auto& fence : uniqueInFlightFences)
    {
        fences.push_back(*fence);
    }
    return device.waitForFences(fences, true, UINT64_MAX);
}

vk::Result FrameManager::init(vk::Device device, FrameManager&& oldFrameManager)
{

    if (!oldFrameManager.isEmpty())
    {
        uniqueInFlightFences = std::move(oldFrameManager.uniqueInFlightFences);
        uniqueImageAvailableSemaphores = std::move(oldFrameManager.uniqueImageAvailableSemaphores);
        framesInFlightCount = oldFrameManager.framesInFlightCount;
        swapchainImageCount = oldFrameManager.swapchainImageCount;

        uniqueRenderFinishedSemaphores.resize(swapchainImageCount);
        for (auto& uniqueRenderFinishedSemaphore : uniqueRenderFinishedSemaphores)
        {
            auto [result, semaphore] = device.createSemaphoreUnique({});

            if (result != vk::Result::eSuccess) return result;

            uniqueRenderFinishedSemaphore = std::move(semaphore);
        }
        return vk::Result::eSuccess;
    }

    uniqueInFlightFences.resize(framesInFlightCount);
    uniqueImageAvailableSemaphores.resize(framesInFlightCount);
    uniqueRenderFinishedSemaphores.resize(swapchainImageCount);

    for (auto& uniqueInFlightFence : uniqueInFlightFences)
    {
        auto [result, fence] = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled});

        if (result != vk::Result::eSuccess) return result;

        uniqueInFlightFence = std::move(fence);
    }

    for (auto& uniqueImageAvailableSemaphore : uniqueImageAvailableSemaphores)
    {
        auto [result, semaphore] = device.createSemaphoreUnique({});

        if (result != vk::Result::eSuccess) return result;

        uniqueImageAvailableSemaphore = std::move(semaphore);
    }

    for (auto& uniqueRenderFinishedSemaphore : uniqueRenderFinishedSemaphores)
    {
        auto [result, semaphore] = device.createSemaphoreUnique({});

        if (result != vk::Result::eSuccess) return result;

        uniqueRenderFinishedSemaphore = std::move(semaphore);
    }

    return vk::Result::eSuccess;
}
} // namespace shuttle