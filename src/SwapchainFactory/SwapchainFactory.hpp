//
// Created by Shagu on 16.06.2026.
//

#ifndef HELLOTRIANGLE_SWAPCHAINFACTORY_HPP
#define HELLOTRIANGLE_SWAPCHAINFACTORY_HPP
#include <iostream>
#include <ostream>

#include "IncludeVulkan.hpp"

#include "VkBootstrap.h"
#include "FrameManager/FrameManager.hpp"
#include "PbrRender/Render.hpp"

namespace shuttle_engine {

    struct Swapchain {
        vk::UniqueSwapchainKHR swapchain;
        vk::Extent2D extent;
        vk::Format format;
        std::vector<vk::Image> images;
        uint32_t imageCount{0};
    };

    struct SwapchainContext {
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
        vk::SurfaceKHR surface;
        uint32_t graphicsQueueFamily;
        uint32_t presentQueueFamily;
    };

    // Просто свободная функция! Никаких классов.
    [[nodiscard]] vk::ResultValue<Swapchain> createSwapchain(
        SwapchainContext const& ctx,
        vk::Extent2D extent,
        vk::SwapchainKHR oldSwapchain = VK_NULL_HANDLE
    ) noexcept;
} // shuttle_engine

#endif //HELLOTRIANGLE_SWAPCHAINFACTORY_HPP
