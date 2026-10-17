//
// Created by Shagu on 22.09.2026.
//
#include "engine_impl.hpp"
#include "VkBootstrap.h"

namespace shuttle::engine {

    vk::ResultValue<SwapchainResourcesHandle> EngineImpl::createSwapchainResources(
        SurfaceHandle surfaceHandle,
        vk::Extent2D extent,
        SwapchainResourcesHandle oldSwapchainResourcesHandle) {

        auto& oldSwapchain = m_swapchainResources.get(oldSwapchainResourcesHandle)->swapchain;
        auto& surface = *m_surfaces.get(surfaceHandle);

        auto vkbSwapchainResult = vkb::SwapchainBuilder{m_physicalDevice, *m_uniqueDevice, *surface}
            .set_old_swapchain(*oldSwapchain)
            .set_desired_extent(extent.width, extent.height)
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_composite_alpha_flags(VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            .build();

        if (!vkbSwapchainResult)
        {
            return {vk::Result{vkbSwapchainResult.vk_result()}, {}};
        }

        auto vkbSwapchain = vkbSwapchainResult.value();
        auto imagesRes = vkbSwapchain.get_images();

        if (!imagesRes.has_value())
        {
            return {vk::Result{imagesRes.vk_result()}, {}};
        }

        std::vector<vk::Image> swapchainImages;
        swapchainImages.reserve(imagesRes.value().size());
        std::ranges::transform(imagesRes.value(), std::back_inserter(swapchainImages),
                               [](VkImage const& image) { return vk::Image{image}; });

        std::vector<vk::UniqueImageView> swapchainImageViews;
        swapchainImageViews.reserve(swapchainImages.size());
        for (auto& image : swapchainImages) {
            auto [createViewResult, view] = m_uniqueDevice->createImageViewUnique({
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = static_cast<vk::Format>(vkbSwapchain.image_format),
                .components = vk::ComponentMapping{
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity
                },
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            });
            if (createViewResult != vk::Result::eSuccess) return {createViewResult, {}};
            swapchainImageViews.push_back(std::move(view));
        }

        std::vector<vk::UniqueSemaphore> swapchainSemaphores;
        swapchainSemaphores.reserve(vkbSwapchain.image_count);
        for (int i = 0; i < vkbSwapchain.image_count; ++i) {
            auto [createSemaphoreResult, semaphore] = m_uniqueDevice->createSemaphoreUnique({}, nullptr, m_dispatcher);
            if (createSemaphoreResult != vk::Result::eSuccess) return {createSemaphoreResult, {}};
            swapchainSemaphores.push_back(std::move(semaphore));
        }

        SwapchainResources swapchain{
            .renderFinishedSemaphores = std::move(swapchainSemaphores),
            .swapchain = vk::UniqueSwapchainKHR{
                vkbSwapchain.swapchain, vk::UniqueHandleTraits<
                    vk::SwapchainKHR, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                    *m_uniqueDevice, nullptr, m_dispatcher
                }
            },
            .extent{
                .width = vkbSwapchain.extent.width,
                .height = vkbSwapchain.extent.height
            },
            .format = static_cast<vk::Format>(vkbSwapchain.image_format),
            .images{std::move(swapchainImages)},
            .imageViews{std::move(swapchainImageViews)},
            .imageCount = vkbSwapchain.image_count
        };

        auto handle = m_swapchainResources.allocate(std::move(swapchain));

        return {vk::Result::eSuccess, handle};
    }


}
