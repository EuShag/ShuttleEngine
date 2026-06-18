//
// Created by Shagu on 15.06.2026.
//

#include "FrameManager.hpp"

namespace shuttle_engine {

    vk::ResultValue<FrameManager> FrameManager::create(
        vk::Device device,
        uint32_t framesInFlightCount, // Чтобы определить количества семафоров для ожидания рендеринга и фенсов для командного буфера
        uint32_t swapchainImageCount,
        FrameManager&& oldFrameManager
    ) {
        FrameManager result{
            framesInFlightCount, swapchainImageCount
        };
        if (auto initResult = result.init(device, std::move(oldFrameManager)); initResult != vk::Result::eSuccess) {
            return {initResult ,{}};
        }
        return {vk::Result::eSuccess, std::move(result)};
    }

    vk::Result FrameManager::prepareFrameSlot(vk::Device device, uint32_t frameIndex) {
        if (auto result = device.waitForFences({*uniqueInFlightFences[frameIndex]}, true, UINT64_MAX); result != vk::Result::eSuccess) return result;
        return device.resetFences({*uniqueInFlightFences[frameIndex]});
    }

    vk::Result FrameManager::waitRenderIdle(vk::Device device) noexcept {
        std::vector<vk::Fence> fences;
        fences.reserve(framesInFlightCount);
        for (uint32_t i = 0; i < framesInFlightCount; i++) {
            fences.push_back(*uniqueInFlightFences[i]);
        }

        return device.waitForFences(fences, true, UINT64_MAX);
    }

    vk::ResultValue<uint32_t> FrameManager::acquireNextImage(
        vk::Device device,
        vk::SwapchainKHR swapchain,
        uint32_t frameIndex) {
        return device.acquireNextImageKHR(swapchain, UINT64_MAX, *uniqueImageAvailableSemaphores[frameIndex]);
    }

    vk::Result FrameManager::submitRenderCommands(vk::Queue graphicsQueue, vk::CommandBuffer cmd, uint32_t frameIndex,
        uint32_t imageIndex) {

        vk::PipelineStageFlags pipelineStagesMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        return graphicsQueue.submit(
            {
                {
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &*uniqueImageAvailableSemaphores[frameIndex],
                    .pWaitDstStageMask = &pipelineStagesMask,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &cmd,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &*uniqueRenderFinishedSemaphores[imageIndex]
                }
            },
            *uniqueInFlightFences[frameIndex]
        );
    }

    vk::Result FrameManager::present(vk::Queue presentQueue, vk::SwapchainKHR swapchain, uint32_t imageIndex) {
        return presentQueue.presentKHR(
            {
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*uniqueRenderFinishedSemaphores[imageIndex],
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &imageIndex
            }
        );
    }

    vk::Result FrameManager::init(
        vk::Device device,
        FrameManager&& oldFrameManager
        ) {

        if (!oldFrameManager.isEmpty()) {
            uniqueInFlightFences = std::move(oldFrameManager.uniqueInFlightFences);
            uniqueImageAvailableSemaphores = std::move(oldFrameManager.uniqueImageAvailableSemaphores);
            framesInFlightCount = oldFrameManager.framesInFlightCount;
            swapchainImageCount = oldFrameManager.swapchainImageCount;

            uniqueRenderFinishedSemaphores.resize(swapchainImageCount);
            for (auto& uniqueRenderFinishedSemaphore : uniqueRenderFinishedSemaphores) {
                auto [result, semaphore] = device.createSemaphoreUnique({});

                if (result != vk::Result::eSuccess) return result;

                uniqueRenderFinishedSemaphore = std::move(semaphore);
            }
            return vk::Result::eSuccess;

        }

        uniqueInFlightFences.resize(framesInFlightCount);
        uniqueImageAvailableSemaphores.resize(framesInFlightCount);
        uniqueRenderFinishedSemaphores.resize(swapchainImageCount);

        for (auto& uniqueInFlightFence : uniqueInFlightFences) {
            auto [result, fence] = device.createFenceUnique(
                {
                    .flags = vk::FenceCreateFlagBits::eSignaled
                }
            );

            if (result != vk::Result::eSuccess) return result;

            uniqueInFlightFence = std::move(fence);
        }

        for (auto& uniqueImageAvailableSemaphore : uniqueImageAvailableSemaphores) {
            auto [result, semaphore] = device.createSemaphoreUnique({});

            if (result != vk::Result::eSuccess) return result;

            uniqueImageAvailableSemaphore = std::move(semaphore);
        }

        for (auto& uniqueRenderFinishedSemaphore : uniqueRenderFinishedSemaphores) {
            auto [result, semaphore] = device.createSemaphoreUnique({});

            if (result != vk::Result::eSuccess) return result;

            uniqueRenderFinishedSemaphore = std::move(semaphore);
        }

        return vk::Result::eSuccess;
    }
} // shuttle_engine