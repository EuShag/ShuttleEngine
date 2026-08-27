#pragma once
#include <vector>
#include "IncludeVulkan.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "PAL/Platform.hpp"

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

vk::ResultValue<vk::UniqueShaderModule> loadAndCreateShaderModuleUnique(vk::Device const& device, char const* filePath);
vk::ResultValue<vk::ShaderModule> loadAndCreateShaderModule(vk::Device const& device, char const* filePath);
bool checkExtensionSupport(std::vector<char const*> const& requiredExtensions);
bool checkLayersSupport(std::vector<char const*> const& requiredLayers);
bool checkExtensionsSupport(vk::PhysicalDevice const& physicalDevice,
                            std::vector<char const*> const& requiredExtensions);
uint32_t findMemoryTypeIndex(vk::PhysicalDevice const& physicalDevice, uint32_t memoryTypeBits,
                             vk::MemoryPropertyFlags requiredProperties);
void strideCopy(void* dst, void const* src, size_t elementSize, size_t elementCount, size_t dstStride,
                size_t srcStride);
void safeScreenshot(void* screenshotBuffer, uint32_t width, uint32_t height);

vk::ResultValue<AttachmentOutput> createAttachmentOutput(
    vk::Device device,
    shuttle::resources::DeviceAllocator const& deviceAllocator,
    AttachmentOutputCreateInfo const& attachmentOutputCreateInfo);

vk::ResultValue<vk::SurfaceKHR> createVulkanSurface(
    vk::Instance instance,
    shuttle::pal::Platform const& platform,
    shuttle::pal::WindowHandle handle,
    vk::detail::DispatchLoaderDynamic const& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER);

vk::ResultValue<vk::UniqueSurfaceKHR> createVulkanSurfaceUnique(
    vk::Instance instance,
    shuttle::pal::Platform const& platform,
    shuttle::pal::WindowHandle handle,
    vk::detail::DispatchLoaderDynamic const& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER);