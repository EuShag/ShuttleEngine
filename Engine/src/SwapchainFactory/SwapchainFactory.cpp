//
// Created by Shagu on 16.06.2026.
//

#include "SwapchainFactory.hpp"

#include <algorithm>

namespace shuttle
{

vk::ResultValue<Swapchain> createSwapchain(SwapchainContext const& ctx, vk::Extent2D extent,
                                           vk::SwapchainKHR oldSwapchain) noexcept
{

    vkb::SwapchainBuilder builder{ctx.physicalDevice, ctx.device, ctx.surface, ctx.graphicsQueueFamily,
                                  ctx.presentQueueFamily};

    auto vkbSwapchainResult = builder.set_old_swapchain(oldSwapchain)
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
    std::ranges::transform(swapchainImages, std::back_inserter(swapchainImageViews),
                           [&](vk::Image const& image) {
                               auto [createViewResult, view] = ctx.device.createImageViewUnique(vk::ImageViewCreateInfo{
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
                               return std::move(view);
                           });

    return {vk::Result::eSuccess,
    {
            .swapchain = vk::UniqueSwapchainKHR{
                vkbSwapchain.swapchain, vk::UniqueHandleTraits<
                    vk::SwapchainKHR, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                        ctx.device, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER
                    }
            },
            .extent{
                .width = vkbSwapchain.extent.width,
                .height = vkbSwapchain.extent.height
            },
            .format = static_cast<vk::Format>(vkbSwapchain.image_format),
            .images{std::move(swapchainImages)},
            .imageViews{std::move(swapchainImageViews)},
            .imageCount = vkbSwapchain.image_count}
        };
}
} // namespace shuttle