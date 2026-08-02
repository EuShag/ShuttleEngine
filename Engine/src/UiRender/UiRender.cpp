//
// Created by Shagu on 14.06.2026.
//

#include "UiRender.hpp"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl2.h>

namespace shuttle
{

thread_local VkResult UiRenderCreateResult;

vk::ResultValue<UiRender> UiRender::create(SdlWindow& window, vk::Instance instance, vk::PhysicalDevice physicalDevice,
                                           vk::Device device, uint32_t queueFamilyIndex, vk::Queue queue,
                                           uint32_t imageCount)
{

    UiRender result;

    std::array poolSizes = {vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 100},
                            vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 100}};

    auto [createDescriptorPoolResult, uniqueDescriptorPool] =
        device.createDescriptorPoolUnique({.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                           .maxSets = 100,
                                           .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                           .pPoolSizes = poolSizes.data()});
    if (createDescriptorPoolResult != vk::Result::eSuccess) return {createDescriptorPoolResult, {}};

    result.uiDescriptorPool = std::move(uniqueDescriptorPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForVulkan(window.getWindow());

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;

    ImGui_ImplVulkan_InitInfo initInfo{
        .ApiVersion = vk::makeApiVersion(0, 1, 4, 0),
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .QueueFamily = queueFamilyIndex,
        .Queue = queue,
        .DescriptorPool = *result.uiDescriptorPool,
        .MinImageCount = imageCount,
        .ImageCount = imageCount,
        .PipelineInfoMain = {.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),
                             .PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                             .colorAttachmentCount = 1,
                                                             .pColorAttachmentFormats = &colorFormat,
                                                             .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT}},
        .UseDynamicRendering = true,
        .Allocator = nullptr,
        .CheckVkResultFn = [](VkResult const result) { UiRenderCreateResult = result; },
        .MinAllocationSize = 2048 * 2048};

    ImGui_ImplVulkan_LoadFunctions(
        VK_API_VERSION_1_3,
        [](const char* function_name, void* user_data)
        {
            // Используем внутренний vkGetInstanceProcAddr, который нашел Vulkan-Hpp
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                static_cast<VkInstance>(*reinterpret_cast<vk::Instance*>(user_data)), function_name);
        },
        &instance); // Передаем адрес вашего vk::Instance в user_data

    ImGui_ImplVulkan_Init(&initInfo);

    return {static_cast<vk::Result>(UiRenderCreateResult), std::move(result)};
}

void UiRender::bindInputEventHandler(SdlLibrary& library)
{
    library.addCustomEventProcessor([](SDL_Event const& event) { ImGui_ImplSDL2_ProcessEvent(&event); });
}

void UiRender::drawUi(IuiPainter&& painter)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    painter.drawUi();
    ImGui::Render();
}

void UiRender::recordDrawCommands(vk::CommandBuffer cmdBuffer) const
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

void UiRender::destroy(vk::Device device)
{
    if (device.waitIdle() != vk::Result::eSuccess) throw std::runtime_error("wait-idle");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}
} // namespace shuttle