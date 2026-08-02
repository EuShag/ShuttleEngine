//
// Created by Shagu on 16.06.2026.
//

#ifndef HELLOTRIANGLE_SWAPCHAINFACTORY_HPP
#define HELLOTRIANGLE_SWAPCHAINFACTORY_HPP

#include <vector>

#include "IncludeVulkan.hpp"
#include "VkBootstrap.h"

namespace shuttle
{

struct Swapchain
{
    vk::UniqueSwapchainKHR swapchain;
    vk::Extent2D extent;
    vk::Format format;
    std::vector<vk::Image> images;
    uint32_t imageCount{0};
};

struct SwapchainContext
{
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
    uint32_t graphicsQueueFamily;
    uint32_t presentQueueFamily;
};

[[nodiscard]] vk::ResultValue<Swapchain> createSwapchain(SwapchainContext const& ctx, vk::Extent2D extent,
                                                         vk::SwapchainKHR oldSwapchain = VK_NULL_HANDLE) noexcept;
} // namespace shuttle

#endif // HELLOTRIANGLE_SWAPCHAINFACTORY_HPP
