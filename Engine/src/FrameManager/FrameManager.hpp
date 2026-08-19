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
    [[nodiscard]] static vk::ResultValue<FrameManager> create(vk::Device device, uint32_t framesInFlightCount,
                                                              uint32_t swapchainImageCount,
                                                              FrameManager&& frameManager = {});

    [[nodiscard]] vk::Result prepareFrameSlot(vk::Device device, uint32_t frameIndex);
    [[nodiscard]] vk::Result beginFrame(vk::Device device, uint32_t frameIndex);


    [[nodiscard]] vk::ResultValue<uint32_t> acquireNextImage(vk::Device device, vk::SwapchainKHR swapchain, uint32_t frameIndex);

    [[nodiscard]] vk::Result submitRenderCommands(vk::Queue graphicsQueue, vk::CommandBuffer cmd, uint32_t frameIndex,
                                                  uint32_t imageIndex);

    [[nodiscard]] vk::Result present(vk::Queue presentQueue, vk::SwapchainKHR swapchain, uint32_t imageIndex);

    [[nodiscard]] vk::Result waitRenderIdle(vk::Device device) const noexcept;

    void addOldDepthAttachment(resources::UniqueAllocatedImage &&oldDepthImage, vk::UniqueImageView &&oldDepthImageView);

    FrameManager() noexcept = default;

    // Удаляем копирование, так как vk::Unique... не копируемы
    FrameManager(const FrameManager&) = delete;
    FrameManager& operator=(const FrameManager&) = delete;

    // Разрешаем перемещение (Move)
    FrameManager(FrameManager&& other) noexcept = default;
    FrameManager& operator=(FrameManager&& other) noexcept = default;

  private:
    FrameManager(uint32_t framesInFlightCount, uint32_t swapchainImageCount) noexcept
        : framesInFlightCount{framesInFlightCount}, swapchainImageCount{swapchainImageCount}
    {
    }

    bool isEmpty() const { return framesInFlightCount == 0; }

    [[nodiscard]] vk::Result init(vk::Device device, FrameManager&& oldFrameManager = {});

    std::vector<vk::UniqueFence> uniqueInFlightFences;
    std::vector<vk::UniqueSemaphore> uniqueImageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> uniqueRenderFinishedSemaphores;
    std::vector<resources::UniqueAllocatedImage> oldDepthImages;
    std::vector<vk::UniqueImageView> oldDepthImageViews;

    uint32_t framesInFlightCount;
    uint32_t swapchainImageCount;
};
} // namespace shuttle

#endif // HELLOTRIANGLE_FRAMEMANAGER_HPP
