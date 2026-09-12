//
// Created by Shagu on 06.09.2026.
//

#include "SwapchainManager.hpp"

namespace shuttle {
    vk::ResultValue<SwapchainManager> SwapchainManager::create(vk::Device device, uint32_t swapchainImageCount) {

        SwapchainManager manager{swapchainImageCount};
        if (auto initResult = manager.init(device); initResult != vk::Result::eSuccess) {
            return {initResult, {}};
        }
        return {vk::Result::eSuccess, std::move(manager)};
    }

    vk::Result SwapchainManager::init(vk::Device device) {
        renderFinishedSemaphores.reserve(swapchainImageCount);
        presentCompleteFences.reserve(swapchainImageCount);

        for (uint32_t i = 0; i < swapchainImageCount; ++i) {
            vk::SemaphoreCreateInfo semaphoreInfo{};
            vk::FenceCreateInfo fenceInfo{
                .flags = vk::FenceCreateFlagBits::eSignaled
            };

            auto [createSemaphoreResult, renderFinishedSemaphore] = device.createSemaphoreUnique(semaphoreInfo);
            if (createSemaphoreResult != vk::Result::eSuccess) {
                return createSemaphoreResult;
            }
            renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
            auto [createFenceResult, presentCompleteFence] = device.createFenceUnique(fenceInfo);
            if (createFenceResult != vk::Result::eSuccess) {
                return createFenceResult;
            }
            presentCompleteFences.push_back(std::move(presentCompleteFence));
        }
        return vk::Result::eSuccess;
    }

    vk::Result SwapchainManager::presentImage(vk::Queue presentQueue, vk::SwapchainKHR swapchain, uint32_t imageIndex) {

        vk::SwapchainPresentFenceInfoKHR presentFenceInfo{
            .swapchainCount = 1,
            .pFences = &*presentCompleteFences[imageIndex]
        };

        return presentQueue.presentKHR({
            .pNext = &presentFenceInfo,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
        });
    }

    vk::Result SwapchainManager::beginPresent(vk::Device device, uint32_t imageIndex) const {
        return device.resetFences({*presentCompleteFences[imageIndex]});
    }

    vk::Result SwapchainManager::waitForAllPresets(vk::Device device, uint64_t timeout) const {
        std::vector<vk::Fence> fences;
        fences.reserve(swapchainImageCount);
        for (auto const& presentCompleteFence : presentCompleteFences) {
            fences.push_back(*presentCompleteFence);
        }
        return device.waitForFences(
            static_cast<uint32_t>(fences.size()),
            fences.data(),
            VK_TRUE,
            timeout
        );
    }

    vk::Result SwapchainManager::getPresentStatus(vk::Device device, uint32_t imageIndex) const {
        return device.getFenceStatus(*presentCompleteFences[imageIndex]);
    }
}
