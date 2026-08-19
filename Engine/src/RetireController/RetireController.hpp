//
// Created by Shagu on 17.06.2026.
//

#ifndef HELLOTRIANGLE_RETIRECONTROLLER_HPP
#define HELLOTRIANGLE_RETIRECONTROLLER_HPP
#include <vector>
#include "IncludeVulkan.hpp"
#include "FrameManager/FrameManager.hpp"
#include "Render/Render.hpp"
#include "SwapchainFactory/SwapchainFactory.hpp"

namespace shuttle
{

struct RetiredSwapchain
{
    vk::UniqueSwapchainKHR swapchain;
    FrameManager retiredFrameManager;

    uint32_t renderMask = 0;  // Текущая маска кадров (стартует с 0)
    uint32_t presentMask = 0; // Текущая маска картинок (стартует с 0)

    uint32_t targetRenderMask;  // Ожидаемая маска (например, 0b11)
    uint32_t targetPresentMask; // Ожидаемая маска (например, 0b111)
};

struct SwapchainResources
{
    Swapchain swapchain;
    FrameManager frameManager;
};

class RetireController
{
  public:
    vk::ResultValue<SwapchainResources> updateSwapchainResources(
        SwapchainContext const& swapchainContext,
        vk::Extent2D const& swapchainExtent,
        uint32_t frameCount,
        SwapchainResources&& oldSwapchainResources);

    void renderRetireUpdate(uint32_t frameIndex);
    void presentRetireUpdate(uint32_t imageIndex);
    void cleanupExpired();

  private:
    std::vector<RetiredSwapchain> retiredSwapchains;
};
} // namespace shuttle

#endif // HELLOTRIANGLE_RETIRECONTROLLER_HPP
