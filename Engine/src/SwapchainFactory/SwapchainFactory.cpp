//
// Created by Shagu on 16.06.2026.
//

#include "SwapchainFactory.hpp"

namespace shuttle_engine {

    vk::ResultValue<Swapchain> createSwapchain(
        SwapchainContext const& ctx,
        vk::Extent2D extent,
        vk::SwapchainKHR oldSwapchain
    ) noexcept {

        vkb::SwapchainBuilder builder{
            ctx.physicalDevice,
            ctx.device,
            ctx.surface,
            ctx.graphicsQueueFamily,
            ctx.presentQueueFamily
        };

        auto vkbSwapchainResult = builder
            .set_old_swapchain(oldSwapchain)
            .set_desired_extent(extent.width, extent.height)
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .build();

        if (!vkbSwapchainResult) {
            return { vk::Result{vkbSwapchainResult.vk_result()}, {} };
        }

        auto vkbSwapchain = vkbSwapchainResult.value();
        auto imagesRes = vkbSwapchainResult.value().get_images();

        if (!imagesRes.has_value()) {
            return {vk::Result{imagesRes.vk_result()}, {}};
        }

        std::vector<vk::Image> swapchainImages;
        swapchainImages.reserve(imagesRes.value().size());
        std::ranges::transform(imagesRes.value(), std::back_inserter(swapchainImages), [](VkImage const& image) {return vk::Image{image}; });

        return { vk::Result::eSuccess, {
                .swapchain = vk::UniqueSwapchainKHR{
                    vkbSwapchain.swapchain,
                    vk::UniqueHandleTraits<vk::SwapchainKHR, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{ctx.device, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER}
                },
                .extent{
                    .width = vkbSwapchain.extent.width,
                    .height = vkbSwapchain.extent.height
                },
                .format = static_cast<vk::Format>(vkbSwapchain.image_format),
                .images{
                    std::move(swapchainImages)
                },
                .imageCount = vkbSwapchain.image_count
            }
        };
    }
} // shuttle_engine