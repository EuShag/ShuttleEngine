//
// Created by Shagu on 06.09.2026.
//

#ifndef SHUTTLEENGINE_SWAPCHAINMANAGER_HPP
#define SHUTTLEENGINE_SWAPCHAINMANAGER_HPP

#include "IncludeVulkan.hpp"

namespace shuttle {
    class SwapchainManager {
    public:
        SwapchainManager() = default;
        SwapchainManager(const SwapchainManager&) = delete;
        SwapchainManager& operator=(const SwapchainManager&) = delete;

        SwapchainManager(SwapchainManager&& other)  noexcept :
            renderFinishedSemaphores(std::move(other.renderFinishedSemaphores)),
            presentCompleteFences(std::move(other.presentCompleteFences)),
            swapchainImageCount(other.swapchainImageCount) {
                other.swapchainImageCount = 0;
        }
        SwapchainManager& operator=(SwapchainManager&& other) noexcept {
            if (this != &other) {
                renderFinishedSemaphores = std::move(other.renderFinishedSemaphores);
                presentCompleteFences = std::move(other.presentCompleteFences);
                swapchainImageCount = other.swapchainImageCount;
                other.swapchainImageCount = 0;
            }
            return *this;
        }

        [[nodiscard]] static vk::ResultValue<SwapchainManager> create(
            vk::Device device,
            uint32_t swapchainImageCount
        );

        [[nodiscard]] vk::Result presentImage(
            vk::Queue presentQueue,
            vk::SwapchainKHR swapchain,
            uint32_t imageIndex
        );

        [[nodiscard]] vk::Result beginPresent(
            vk::Device device,
            uint32_t imageIndex
        ) const;

        [[nodiscard]] vk::Result waitForAllPresets(
            vk::Device device,
            uint64_t timeout = UINT64_MAX) const;

        [[nodiscard]] vk::Result getPresentStatus(
            vk::Device device,
            uint32_t imageIndex) const;

        [[nodiscard]] vk::Semaphore getRenderFinishedSemaphore(uint32_t imageIndex) const {
            return *renderFinishedSemaphores[imageIndex];
        }

        [[nodiscard]] vk::Fence getPresentCompleteFence(uint32_t imageIndex) const {
            return *presentCompleteFences[imageIndex];
        }

        [[nodiscard]] uint32_t getSwapchainImageCount() const {
            return swapchainImageCount;
        }

    private:

        SwapchainManager(uint32_t swapchainImageCount) noexcept
            : swapchainImageCount(swapchainImageCount) {}

        vk::Result init(vk::Device device);

        std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
        std::vector<vk::UniqueFence> presentCompleteFences;
        uint32_t swapchainImageCount{};
    };
}

#endif //SHUTTLEENGINE_SWAPCHAINMANAGER_HPP
