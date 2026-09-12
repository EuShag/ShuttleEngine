//
// Created by Shagu on 15.06.2026.
//

#ifndef HELLOTRIANGLE_FRAMEMANAGER_HPP
#define HELLOTRIANGLE_FRAMEMANAGER_HPP
#include <array>
#include <vector>
#include "IncludeVulkan.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"

namespace shuttle
{
    class FrameManager
    {
    public:
        [[nodiscard]] static vk::ResultValue<FrameManager> create(vk::Device device, uint32_t framesInFlightCount, vk::detail::DispatchLoaderDynamic const& dldi);

        [[nodiscard]] vk::Result prepareFrameSlot(vk::Device device, uint32_t frameIndex, vk::detail::DispatchLoaderDynamic const& dldi);
        [[nodiscard]] vk::ResultValue<uint32_t> acquireNextFrameSlot(vk::Device device, vk::detail::DispatchLoaderDynamic const& dldi);
        [[nodiscard]] vk::ResultValue<uint32_t> acquireNextImage(vk::Device device, vk::SwapchainKHR swapchain, uint32_t frameIndex, vk::detail::DispatchLoaderDynamic const& dldi);
        [[nodiscard]] vk::Result beginFrame(vk::Device device, uint32_t frameIndex, vk::detail::DispatchLoaderDynamic const &dldi);
        [[nodiscard]] vk::Result waitRenderIdle(vk::Device device, vk::detail::DispatchLoaderDynamic const &dldi) const noexcept;

        FrameManager() noexcept = default;

        // Удаляем копирование, так как vk::Unique... не копируемы
        FrameManager(const FrameManager&) = delete;
        FrameManager& operator=(const FrameManager&) = delete;

        // Разрешаем перемещение (Move)
        FrameManager(FrameManager&& other) noexcept = default;
        FrameManager& operator=(FrameManager&& other) noexcept = default;

    private:
        FrameManager(uint32_t framesInFlightCount) noexcept
        : framesInFlightCount{framesInFlightCount} {}

        [[nodiscard]] bool isEmpty() const { return framesInFlightCount == 0; }

        [[nodiscard]] vk::Result init(vk::Device device, vk::detail::DispatchLoaderDynamic const& dldi);

        std::vector<vk::UniqueFence> uniqueInFlightFences;
        std::vector<vk::UniqueSemaphore> uniqueImageAvailableSemaphores;

        uint32_t framesInFlightCount{};
    };
} // namespace shuttle

#endif // HELLOTRIANGLE_FRAMEMANAGER_HPP
