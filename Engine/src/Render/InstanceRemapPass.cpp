//
// Created by Shagu on 05.08.2026.
//

#include "InstanceRemapPass.hpp"

#include <fstream>

#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine::render
{
    namespace
    {
        struct alignas(16) InstanceRemapPassData
        {
            vk::DeviceAddress indirectDrawCommandsBufferAddress{};
            vk::DeviceAddress instanceRemapBufferAddress{};
            vk::DeviceAddress meshInstanceCursorBufferAddress{};

            uint32_t drawableCount{};
            uint32_t meshCount{};

            uint32_t reserved0{};
            uint32_t reserved1{};
            uint32_t reserved2{};
        };

        static_assert(sizeof(InstanceRemapPassData) % 16 == 0);

        vk::ResultValue<vk::UniquePipeline> createInstanceRemapPipeline(
            vk::Device device,
            vk::PipelineLayout pipelineLayout)
        {
            auto [createShaderModuleResult, shaderModule] =
                loadAndCreateShaderModuleUnique(
                    device,
                    "../shaders/instanceRemapPass.comp.spv");

            if (createShaderModuleResult != vk::Result::eSuccess)
            {
                return {createShaderModuleResult, {}};
            }

            vk::PipelineShaderStageCreateInfo stageInfo{
                .stage = vk::ShaderStageFlagBits::eCompute,
                .module = *shaderModule,
                .pName = "main"
            };

            vk::ComputePipelineCreateInfo pipelineInfo{
                .stage = stageInfo,
                .layout = pipelineLayout
            };

            return device.createComputePipelineUnique(
                {},
                pipelineInfo);
        }
    }

    vk::ResultValue<InstanceRemapPass> InstanceRemapPass::create(
        vk::Device device,
        vk::PipelineLayout pipelineLayout)
    {
        auto [createPipelineResult, pipeline] =
            createInstanceRemapPipeline(
                device,
                pipelineLayout);

        if (createPipelineResult != vk::Result::eSuccess)
        {
            return {createPipelineResult, {}};
        }

        return {
            vk::Result::eSuccess,
            InstanceRemapPass{
                std::move(pipeline),
                pipelineLayout
            }
        };
    }

    void InstanceRemapPass::writeRenderCommands(
        vk::CommandBuffer cmdBuffer,
        InstanceRemapPassInfo const& info) const
    {
        if (info.drawableCount == 0 ||
            info.meshCount == 0)
        {
            return;
        }

        const vk::DeviceSize meshInstanceCursorBufferSize =
            static_cast<vk::DeviceSize>(info.meshCount) *
            sizeof(uint32_t);

        if (info.clearMeshInstanceCursorBuffer)
        {
            cmdBuffer.fillBuffer(
                info.meshInstanceCursorBuffer,
                0,
                meshInstanceCursorBufferSize,
                0u);

            vk::BufferMemoryBarrier2 clearToComputeBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask =
                    vk::AccessFlagBits2::eShaderRead |
                    vk::AccessFlagBits2::eShaderWrite,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .buffer = info.meshInstanceCursorBuffer,
                .offset = 0,
                .size = meshInstanceCursorBufferSize
            };

            cmdBuffer.pipelineBarrier2(
                vk::DependencyInfo{
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &clearToComputeBarrier
                });
        }

        InstanceRemapPassData passData{
            .indirectDrawCommandsBufferAddress =
                info.indirectDrawCommandsBufferAddress,

            .instanceRemapBufferAddress =
                info.instanceRemapBufferAddress,

            .meshInstanceCursorBufferAddress =
                info.meshInstanceCursorBufferAddress,

            .drawableCount = info.drawableCount,
            .meshCount = info.meshCount,

            .reserved0 = 0,
            .reserved1 = 0,
            .reserved2 = 0
        };

        cmdBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *pipeline);

        cmdBuffer.pushConstants(
            pipelineLayout,
             vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            PassSpecificDataOffset,
            sizeof(InstanceRemapPassData),
            &passData);

        const uint32_t groupCount =
            (info.drawableCount + WorkGroupSize - 1u) /
            WorkGroupSize;

        cmdBuffer.dispatch(
            groupCount,
            1,
            1);
    }
}