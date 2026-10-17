#pragma once
#include "include_vulkan.hpp"
#include "device_allocator/device_allocator.hpp"
#include "pal/platform.hpp"

struct AttachmentOutput {
    shuttle::resources::UniqueAllocatedImage image;
    vk::UniqueImageView view;
};

struct AttachmentOutputCreateInfo {
    vk::Format format;
    vk::ImageUsageFlags imageUsage;
    vk::Extent2D imageSize;
    vk::ImageAspectFlags imageAspectFlags;
};

vk::ResultValue<vk::UniqueShaderModule> loadAndCreateShaderModuleUnique(vk::Device const& device, vk::detail::DispatchLoaderDynamic const& dld, char const* filePath);
vk::ResultValue<vk::ShaderModule> loadAndCreateShaderModule(vk::Device const& device, vk::detail::DispatchLoaderDynamic const& dld, char const* filePath);
void strideCopy(void* dst, void const* src, size_t elementSize, size_t elementCount, size_t dstStride,
                size_t srcStride);
void safeScreenshot(void* screenshotBuffer, uint32_t width, uint32_t height);

vk::ResultValue<AttachmentOutput> createAttachmentOutput(
    vk::Device device,
    vk::detail::DispatchLoaderDynamic const& dld,
    shuttle::resources::DeviceAllocator const& deviceAllocator,
    AttachmentOutputCreateInfo const& attachmentOutputCreateInfo);

vk::ResultValue<vk::SurfaceKHR> createVulkanSurface(
    vk::Instance instance,
    shuttle::pal::Platform const& platform,
    shuttle::pal::WindowHandle handle,
    vk::detail::DispatchLoaderDynamic const& dispatcher);

vk::ResultValue<vk::UniqueSurfaceKHR> createVulkanSurfaceUnique(
    vk::Instance instance,
    shuttle::pal::Platform const& platform,
    shuttle::pal::WindowHandle handle,
    vk::detail::DispatchLoaderDynamic const& dispatcher);

vk::Result submitRenderCommands(
    vk::Queue graphicsQueue,
    vk::CommandBuffer commandBuffer,
    vk::detail::DispatchLoaderDynamic const& dispatcher,
    vk::Semaphore renderFinishedSemaphore,
    vk::Semaphore imageAvailableSemaphore,
    vk::Fence inFlightFences
);