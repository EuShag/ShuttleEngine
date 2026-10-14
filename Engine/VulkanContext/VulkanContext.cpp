//
// Created by Shagu on 02.10.2026.
//

#include "VulkanContext.hpp"

#include <fstream>

namespace shuttle::engine {
    vk::ResultValue<vk::SurfaceKHR> VulkanContext::createSurface(pal::Platform const &platform,
        pal::WindowHandle handle) {
        VkInstance vk_instance = *m_uniqueInstance;
        VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
        PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr = m_dispatcher.vkGetInstanceProcAddr;

        auto result = platform.createVulkanSurface(
            handle, vk_instance,
            &vk_surface,
            reinterpret_cast<void *>(vk_get_instance_proc_addr));

        auto vk_result = static_cast<vk::Result>(result);

        return {vk_result, vk_surface};
    }

    vk::ResultValue<vk::UniqueSurfaceKHR> VulkanContext::createSurfaceUnique(pal::Platform const &platform,
        pal::WindowHandle handle) {
        auto [result, surface] = createSurface(platform, handle);

        return { result, vk::UniqueSurfaceKHR{surface, {*m_uniqueInstance, nullptr, m_dispatcher} } };
    }

    vk::ResultValue<vk::SwapchainKHR> VulkanContext::createSwapchain(
        vk::SwapchainCreateInfoKHR const &swapchainCreateInfo) {
        return m_uniqueDevice->createSwapchainKHR(swapchainCreateInfo, nullptr, m_dispatcher);
    }

    vk::ResultValue<vk::UniqueSwapchainKHR> VulkanContext::createSwapchainUnique(
        vk::SwapchainCreateInfoKHR const &swapchainCreateInfo) {
        auto [result, swapchain] = createSwapchain(swapchainCreateInfo);

        return { result, vk::UniqueSwapchainKHR{swapchain, {*m_uniqueDevice, nullptr, m_dispatcher} } };
    }

    vk::ResultValue<vk::ShaderModule> VulkanContext::createShaderModule(std::filesystem::path const &shaderPath) {
        std::fstream shaderModuleFile(shaderPath, std::ios::binary | std::ios::ate | std::ios::in);
        if (!shaderModuleFile.is_open())
        {
            throw std::runtime_error("Failed to open fragment shader file");
        }
        auto const shaderModuleFileSize = shaderModuleFile.tellg();
        std::vector<char> fragmentShaderCode(shaderModuleFileSize);
        shaderModuleFile.seekg(0);
        shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

        return m_uniqueDevice->createShaderModule({
            .codeSize = fragmentShaderCode.size(),
            .pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())},
            nullptr,
            m_dispatcher
        );
    }

    vk::ResultValue<vk::UniqueShaderModule> VulkanContext::createShaderModuleUnique(std::filesystem::path const &shaderPath) {
        auto [result, shaderModule] = createShaderModule(shaderPath);

        return { result, vk::UniqueShaderModule{shaderModule, {*m_uniqueDevice, nullptr, m_dispatcher} } };
    }

    vk::ResultValue<vk::Pipeline> VulkanContext::createComputePipeline(
        vk::ComputePipelineCreateInfo const &pipelineCreateInfos) {
        return m_uniqueDevice->createComputePipeline(nullptr, pipelineCreateInfos, nullptr, m_dispatcher);
    }

    vk::ResultValue<vk::UniquePipeline> VulkanContext::createComputePipelineUnique(
        vk::ComputePipelineCreateInfo const &pipelineCreateInfos) {
        return m_uniqueDevice->createComputePipelineUnique(nullptr, pipelineCreateInfos, nullptr, m_dispatcher);
    }

    vk::ResultValue<vk::Pipeline> VulkanContext::createGraphicsPipeline(
        vk::GraphicsPipelineCreateInfo const &pipelineCreateInfo) {
        return m_uniqueDevice->createGraphicsPipeline(nullptr, pipelineCreateInfo, nullptr, m_dispatcher);
    }
    vk::ResultValue<vk::UniquePipeline> VulkanContext::createGraphicsPipelineUnique(
        vk::GraphicsPipelineCreateInfo const &pipelineCreateInfo) {
        return m_uniqueDevice->createGraphicsPipelineUnique(nullptr, pipelineCreateInfo, nullptr, m_dispatcher);
    }

    vk::ResultValue<resources::DeviceAllocator> VulkanContext::createDeviceAllocator() {
        return resources::DeviceAllocator::create(
            *m_uniqueInstance,
            *m_uniqueDevice,
            m_physicalDevice,
            m_dispatcher);
    }

    vk::ResultValue<DedicatedCommandPool> VulkanContext::createDedicatedCommandPool() {
        return DedicatedCommandPool::create(*m_uniqueDevice, m_graphicsQueueFamilyIndex, &m_dispatcher);
    }
}
