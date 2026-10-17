#include "vulkan_helper_functions.hpp"

#include <chrono>
#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <filesystem>
#include <iostream>
#include <stb_image_write.h>

namespace
{

std::filesystem::path makeScreenshotFileName()
{
    namespace fs = std::filesystem;

    fs::create_directories("../resources/screenshots");

    auto now = std::chrono::system_clock::now();

    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::stringstream ss;

    ss << "../resources/screenshots/screenshot_" << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S") << ".png";

    return ss.str();
}

} // namespace

vk::ResultValue<vk::UniqueShaderModule> loadAndCreateShaderModuleUnique(vk::Device const& device, vk::detail::DispatchLoaderDynamic const& dld, char const* filePath)
{
    std::fstream shaderModuleFile(filePath, std::ios::binary | std::ios::ate | std::ios::in);
    if (!shaderModuleFile.is_open())
    {
        throw std::runtime_error("Failed to open fragment shader file");
    }
    auto const shaderModuleFileSize = shaderModuleFile.tellg();
    std::vector<char> fragmentShaderCode(shaderModuleFileSize);
    shaderModuleFile.seekg(0);
    shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

    return device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
        .codeSize = fragmentShaderCode.size(), .pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())}, nullptr, dld);
}

vk::ResultValue<vk::ShaderModule> loadAndCreateShaderModule(vk::Device const& device, vk::detail::DispatchLoaderDynamic const& dld, char const* filePath)
{
    std::fstream shaderModuleFile(filePath, std::ios::binary | std::ios::ate | std::ios::in);
    if (!shaderModuleFile.is_open())
    {
        throw std::runtime_error("Failed to open fragment shader file");
    }
    auto const shaderModuleFileSize = shaderModuleFile.tellg();
    std::vector<char> fragmentShaderCode(shaderModuleFileSize);
    shaderModuleFile.seekg(0);
    shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

    return device.createShaderModule(vk::ShaderModuleCreateInfo{
        .codeSize = fragmentShaderCode.size(), .pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())}, nullptr, dld);
}

void strideCopy(void* dst, void const* src, size_t elementSize, size_t elementCount, size_t dstStride, size_t srcStride)
{
    char* dstChar = static_cast<char*>(dst);
    char const* srcChar = static_cast<char const*>(src);
    for (size_t i = 0; i < elementCount; ++i)
    {
        std::memcpy(dstChar, srcChar, elementSize);
        dstChar += dstStride;
        srcChar += srcStride;
    }
}

void safeScreenshot(void* screenshotBuffer, uint32_t width, uint32_t height)
{
    auto filepath = makeScreenshotFileName();
    // Здесь можно добавить код для сохранения скриншота в файл с именем filename
    // Создадим файл по новому пути

    auto _width = static_cast<int32_t>(width);
    auto _height = static_cast<int32_t>(height);

    std::cout << "Safe Screenshot (" << _width << "x" << _height << ")" << "To" << filepath << " From "
              << screenshotBuffer << std::endl;

    for (size_t i = 0; i < width * height; ++i)
    {
        std::swap(static_cast<uint8_t*>(screenshotBuffer)[4 * i + 0],
                  static_cast<uint8_t*>(screenshotBuffer)[4 * i + 2]);
    }

    auto result = stbi_write_png(filepath.string().c_str(), _width, _height, 4, screenshotBuffer, _width * 4);
    if (result == 0)
    {
        std::cerr << "Failed to save screenshot to " << filepath << std::endl;
    }
}

vk::ResultValue<AttachmentOutput> createAttachmentOutput(
    vk::Device device,
    vk::detail::DispatchLoaderDynamic const& dld,
    shuttle::resources::DeviceAllocator const &deviceAllocator,
    AttachmentOutputCreateInfo const &attachmentOutputCreateInfo) {

    auto [createImageResult, image] = deviceAllocator.createAndAllocateImageUnique({
        .imageType = vk::ImageType::e2D,
        .format = attachmentOutputCreateInfo.format,
        .extent = vk::Extent3D{
            .width = attachmentOutputCreateInfo.imageSize.width,
            .height = attachmentOutputCreateInfo.imageSize.height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = attachmentOutputCreateInfo.imageUsage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined },
    shuttle::resources::MemoryUsage::eGpuOnly);

    if (createImageResult != vk::Result::eSuccess) {
        return {createImageResult, {}};
    }
    auto [createImageViewResult, imageView] = device.createImageViewUnique({
            .image = *image,
            .viewType = vk::ImageViewType::e2D,
            .format = attachmentOutputCreateInfo.format,
            .subresourceRange = {
                .aspectMask = attachmentOutputCreateInfo.imageAspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1}
        }, nullptr, dld);
    if (createImageViewResult != vk::Result::eSuccess) {
        return {createImageViewResult, {}};
    }

    return {vk::Result::eSuccess, {
        .image = std::move(image),
        .view = std::move(imageView),
    }};
}

vk::ResultValue<vk::SurfaceKHR> createVulkanSurface(
    vk::Instance instance,
    shuttle::pal::Platform const& platform,
    shuttle::pal::WindowHandle handle,
    vk::detail::DispatchLoaderDynamic const &dispatcher) {

    VkInstance vk_instance = instance;
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr = dispatcher.vkGetInstanceProcAddr;

    auto result = platform.createVulkanSurface(
        handle, vk_instance,
        &vk_surface,
        reinterpret_cast<void *>(vk_get_instance_proc_addr));

    auto vk_result = static_cast<vk::Result>(result);

    return {vk_result, vk_surface};
}

vk::ResultValue<vk::UniqueSurfaceKHR> createVulkanSurfaceUnique(
    vk::Instance instance,
    shuttle::pal::Platform const& platform,
    shuttle::pal::WindowHandle handle,
    vk::detail::DispatchLoaderDynamic const &dispatcher) {

    auto [result, surface] = createVulkanSurface(instance, platform, handle, dispatcher);

    return { result, vk::UniqueSurfaceKHR(surface, vk::detail::ObjectDestroy(instance, nullptr, dispatcher)) };
}

vk::Result submitRenderCommands(
    vk::Queue graphicsQueue,
    vk::CommandBuffer commandBuffer,
    vk::detail::DispatchLoaderDynamic const &dld,
    vk::Semaphore renderFinishedSemaphore,
    vk::Semaphore imageAvailableSemaphore,
    vk::Fence inFlightFence) {

    vk::SemaphoreSubmitInfo waitSemaphoreSubmitInfo{
        .semaphore = imageAvailableSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands
    };

    vk::SemaphoreSubmitInfo signalSemaphoreSubmitInfo{
        .semaphore = renderFinishedSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
    };

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
        .commandBuffer = commandBuffer
    };

    return graphicsQueue.submit2(
        {{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo,
        }},
        inFlightFence, dld
    );
}
