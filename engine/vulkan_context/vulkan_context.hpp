//
// Created by Shagu on 02.10.2026.
//

#ifndef SHUTTLEENGINE_VULKANCONTEXT_HPP
#define SHUTTLEENGINE_VULKANCONTEXT_HPP

#include <filesystem>

#include "concurrentqueue.h"
#include "device_allocator/device_allocator.hpp"
#include "pal/common/window/window_base.hpp"
#include "render/render.hpp"
#include "command_buffer_provider/command_buffer_provider.hpp"
#include "command_buffer_provider/command_buffer_recorder.hpp"
#include "command_buffer_provider/command_buffer_token.hpp"
#include "command_buffer_provider/dedicated_command_pool.hpp"
#include <async/mi_allocator.hpp>

namespace shuttle::engine {
    using async::MiUniquePtr;

    class CommandBufferRecorder;
    class CommandBufferToken;

    /*
     * Набор прокси классов для управления ресурсами Vulkan с использованием RAII.
     */
    class VulkanContext;

    class VulkanQueue {
    public:
        VulkanQueue(vk::Queue queue, vk::detail::DispatchLoaderDynamic const* dispatcher)
            : m_queue(queue), m_dispatch(dispatcher) {}

        [[nodiscard]] vk::Result submit2(vk::ArrayProxy<const vk::SubmitInfo2> const& submits, vk::Fence fence = nullptr) const {
            return m_queue.submit2(submits, fence, *m_dispatch);
        }
        [[nodiscard]] vk::Result present(vk::PresentInfoKHR const& presentInfo) const {
            return m_queue.presentKHR(presentInfo, *m_dispatch);
        }
        [[nodiscard]] vk::Result waitIdle() const {
            return m_queue.waitIdle(*m_dispatch);
        }

    private:
        vk::Queue m_queue{};
        vk::detail::DispatchLoaderDynamic const* m_dispatch{};
    };

    class VulkanContext {
    public:
        VulkanContext();

        [[nodiscard]] vk::ResultValue<MiUniquePtr<VulkanContext>> create();
        [[nodiscard]] vk::ResultValue<VulkanContext*> createInPlace(void* memory) noexcept;
        [[nodiscard]] vk::Result init();

        [[nodiscard]] vk::ResultValue<vk::SurfaceKHR> createSurface(
            pal::Platform const& platform,
            pal::WindowHandle handle);

        [[nodiscard]] vk::ResultValue<vk::UniqueSurfaceKHR> createSurfaceUnique(
            pal::Platform const& platform,
            pal::WindowHandle handle);

        [[nodiscard]] vk::ResultValue<vk::SwapchainKHR> createSwapchain(
            vk::SwapchainCreateInfoKHR const& swapchainCreateInfo
        );
        [[nodiscard]] vk::ResultValue<vk::UniqueSwapchainKHR> createSwapchainUnique(
            vk::SwapchainCreateInfoKHR const& swapchainCreateInfo
        );

        [[nodiscard]] vk::ResultValue<vk::ShaderModule> createShaderModule(std::filesystem::path const& shaderPath);
        [[nodiscard]] vk::ResultValue<vk::UniqueShaderModule> createShaderModuleUnique(std::filesystem::path const& shaderPath);

        [[nodiscard]] vk::ResultValue<vk::Pipeline> createComputePipeline(vk::ComputePipelineCreateInfo const &pipelineCreateInfos);
        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createComputePipelineUnique(vk::ComputePipelineCreateInfo const &pipelineCreateInfos);

        [[nodiscard]] vk::ResultValue<vk::Pipeline> createGraphicsPipeline(vk::GraphicsPipelineCreateInfo const& pipelineCreateInfo);
        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createGraphicsPipelineUnique(vk::GraphicsPipelineCreateInfo const& pipelineCreateInfo);

        [[nodiscard]] vk::ResultValue<resources::DeviceAllocator> createDeviceAllocator();

        template<typename Func> requires std::invocable<Func, CommandBufferRecorder const&>
        [[nodiscard]] vk::ResultValue<CommandBufferToken> writeCommandBuffer(vk::CommandBufferBeginInfo beginInfo, Func&& func) {
            return m_commandBufferProvider->writeCommandBuffer(beginInfo, std::forward<Func>(func));
        }

        [[nodiscard]] vk::ResultValue<DedicatedCommandPool> createDedicatedCommandPool();

    private:
        MiUniquePtr<CommandBufferProvider> m_commandBufferProvider;
        vk::detail::DynamicLoader m_dynamicLoader{};
        vk::detail::DispatchLoaderDynamic m_dispatcher{};
        vk::UniqueInstance m_uniqueInstance{};
        vk::PhysicalDevice m_physicalDevice{};
        vk::UniqueDevice m_uniqueDevice{};
        render::DescriptorHeapSet m_descriptorHeapSet{};
        vk::UniqueCommandBuffer m_commandBuffer{};
        vk::Queue m_graphicsQueue{};
        uint32_t m_graphicsQueueFamilyIndex = 0;
        vk::UniqueCommandPool m_commandPool{};
        vk::UniqueDescriptorPool m_descriptorPool{};
    };
}

#endif //SHUTTLEENGINE_VULKANCONTEXT_HPP
