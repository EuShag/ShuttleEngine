//
// Created by Shagu on 05.10.2026.
//

#include "command_buffer_recorder.hpp"

namespace shuttle::engine {

    void CommandBufferRecorder::dispatch(uint32_t x, uint32_t y, uint32_t z) const noexcept {
        m_commandBuffer.dispatch(x, y, z, *m_dispatcher);
    }

    void CommandBufferRecorder::beginRendering(vk::RenderingInfo const &renderPassBeginInfo) const noexcept {
        m_commandBuffer.beginRendering(renderPassBeginInfo, *m_dispatcher);
    }

    void CommandBufferRecorder::endRendering() const noexcept {
        m_commandBuffer.endRendering(*m_dispatcher);
    }

    void CommandBufferRecorder::pushConstants(vk::PipelineLayout layout, vk::ShaderStageFlags stageFlags,
        uint32_t offset, uint32_t size, const void *pValues) const noexcept {
        m_commandBuffer.pushConstants(layout, stageFlags, offset, size, pValues, *m_dispatcher);
    }

    void CommandBufferRecorder::pipelineBarrier2(vk::DependencyInfo const &dependencyInfo) const noexcept {
        m_commandBuffer.pipelineBarrier2(dependencyInfo, *m_dispatcher);
    }

    void CommandBufferRecorder::bindDescriptorSets(vk::PipelineBindPoint pipelineBindPoint, vk::PipelineLayout layout,
        uint32_t firstSet, vk::ArrayProxy<const vk::DescriptorSet> const &descriptorSets,
        vk::ArrayProxy<const uint32_t> const &dynamicOffsets) const noexcept {
        m_commandBuffer.bindDescriptorSets(pipelineBindPoint, layout, firstSet, descriptorSets, dynamicOffsets, *m_dispatcher);
    }

    void CommandBufferRecorder::copyBuffer2(vk::CopyBufferInfo2 const &copyInfo) const noexcept {
        m_commandBuffer.copyBuffer2(copyInfo, *m_dispatcher);
    }

    void CommandBufferRecorder::copyImage2(vk::CopyImageInfo2 const &copyInfo) const noexcept {
        m_commandBuffer.copyImage2(copyInfo, *m_dispatcher);
    }

    void CommandBufferRecorder::copyBufferToImage2(vk::CopyBufferToImageInfo2 const &copyInfo) const noexcept {
        m_commandBuffer.copyBufferToImage2(copyInfo, *m_dispatcher);
    }

    void CommandBufferRecorder::copyImageToBuffer2(vk::CopyImageToBufferInfo2 const &copyInfo) const noexcept {
        m_commandBuffer.copyImageToBuffer2(copyInfo, *m_dispatcher);
    }

    void CommandBufferRecorder::fillBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size,
        uint32_t data) const noexcept {
        m_commandBuffer.fillBuffer(buffer, offset, size, data, *m_dispatcher);
    }

    void CommandBufferRecorder::clearAttachments(vk::ArrayProxy<const vk::ClearAttachment> const &clearAttachments,
        vk::ArrayProxy<const vk::ClearRect> const &clearRects) const noexcept {
        m_commandBuffer.clearAttachments(clearAttachments, clearRects, *m_dispatcher);
    }

    void CommandBufferRecorder::clearColorImage(vk::Image image, vk::ImageLayout imageLayout,
        vk::ClearColorValue const &color, vk::ArrayProxy<const vk::ImageSubresourceRange> const &ranges) const noexcept {
        m_commandBuffer.clearColorImage(image, imageLayout, color, ranges, *m_dispatcher);
    }

    void CommandBufferRecorder::clearDepthStencilImage(vk::Image image, vk::ImageLayout imageLayout,
        vk::ClearDepthStencilValue const &depthStencil,
        vk::ArrayProxy<const vk::ImageSubresourceRange> const &ranges) const noexcept {
        m_commandBuffer.clearDepthStencilImage(image, imageLayout, depthStencil, ranges, *m_dispatcher);
    }

    void CommandBufferRecorder::bindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType indexType) const noexcept
    {
        m_commandBuffer.bindIndexBuffer(buffer, offset, indexType, *m_dispatcher);
    }
}
