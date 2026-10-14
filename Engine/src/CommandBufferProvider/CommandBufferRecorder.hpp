//
// Created by Shagu on 05.10.2026.
//

#ifndef SHUTTLEENGINE_COMMAND_BUFFER_RECORDER_HPP
#define SHUTTLEENGINE_COMMAND_BUFFER_RECORDER_HPP

#include "IncludeVulkan.hpp"

namespace shuttle::engine {
    class CommandBufferRecorder {
    public:
        CommandBufferRecorder(vk::CommandBuffer commandBuffer, vk::detail::DispatchLoaderDynamic const* dispatcher)
            : m_commandBuffer(commandBuffer), m_dispatcher(dispatcher) {}

        void dispatch(uint32_t x, uint32_t y, uint32_t z) const noexcept;
        void beginRendering(vk::RenderingInfo const& renderPassBeginInfo) const noexcept;
        void endRendering() const noexcept;
        void pushConstants(vk::PipelineLayout layout, vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const noexcept;
        void pipelineBarrier2(vk::DependencyInfo const& dependencyInfo) const noexcept;
        void bindDescriptorSets(
            vk::PipelineBindPoint pipelineBindPoint,
            vk::PipelineLayout layout, uint32_t firstSet,
            vk::ArrayProxy<const vk::DescriptorSet> const& descriptorSets,
            vk::ArrayProxy<const uint32_t> const& dynamicOffsets) const noexcept;
        void copyBuffer2(vk::CopyBufferInfo2 const& copyInfo) const noexcept;
        void copyImage2(vk::CopyImageInfo2 const& copyInfo) const noexcept;
        void copyBufferToImage2(vk::CopyBufferToImageInfo2 const& copyInfo) const noexcept;
        void copyImageToBuffer2(vk::CopyImageToBufferInfo2 const& copyInfo) const noexcept;
        void fillBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size, uint32_t data) const noexcept;
        void clearAttachments(vk::ArrayProxy<const vk::ClearAttachment> const& clearAttachments, vk::ArrayProxy<const vk::ClearRect> const& clearRects) const noexcept;
        void clearColorImage(vk::Image image, vk::ImageLayout imageLayout, vk::ClearColorValue const& color, vk::ArrayProxy<const vk::ImageSubresourceRange> const& ranges) const noexcept;
        void clearDepthStencilImage(vk::Image image, vk::ImageLayout imageLayout,
                                    vk::ClearDepthStencilValue const& depthStencil, vk::ArrayProxy<const vk::ImageSubresourceRange> const& ranges) const noexcept;
        void bindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType indexType) const noexcept;

        // Запрещаем копирование и перемещение. Этот класс должен использоваться только в пределах функции, которая его создает.
        CommandBufferRecorder(CommandBufferRecorder const&) = delete;
        CommandBufferRecorder& operator=(CommandBufferRecorder const&) = delete;
        CommandBufferRecorder(CommandBufferRecorder&&) = delete;
        CommandBufferRecorder& operator=(CommandBufferRecorder&&) = delete;

    private:
        vk::CommandBuffer m_commandBuffer{};
        vk::detail::DispatchLoaderDynamic const* m_dispatcher{};
    };
}

#endif //SHUTTLEENGINE_COMMAND_BUFFER_RECORDER_HPP
