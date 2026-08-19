//
// Created by Shagu on 05.08.2026.
//

#include "MeshInstancesCountPass.hpp"

#include <fstream>
#include <filesystem>

#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine::render
{
    namespace
    {
        struct alignas(16) MeshInstancesCountPassData
        {
            vk::DeviceAddress indirectDrawCommandsBufferAddress{};
        };

        static_assert(sizeof(MeshInstancesCountPassData) % 16 == 0);

        vk::ResultValue<vk::UniquePipeline> createMeshInstancesCountPipeline(
            vk::Device device,
            vk::PipelineLayout pipelineLayout)
        {
            auto [createShaderModuleResult, shaderModule] =
                loadAndCreateShaderModuleUnique(
                    device,
                    "../shaders/meshInstancesCount.comp.spv");

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

    vk::ResultValue<MeshInstancesCountPass> MeshInstancesCountPass::create(
        vk::Device device,
        vk::PipelineLayout pipelineLayout)
    {
        auto [createPipelineResult, pipeline] =
            createMeshInstancesCountPipeline(
                device,
                pipelineLayout);

        if (createPipelineResult != vk::Result::eSuccess)
        {
            return {createPipelineResult, {}};
        }

        return {
            vk::Result::eSuccess,
            MeshInstancesCountPass{
                std::move(pipeline),
                pipelineLayout
            }
        };
    }

    void MeshInstancesCountPass::writeRenderCommands(
        vk::CommandBuffer cmdBuffer,
        MeshInstancesCountPassInfo const& info) const
    {
        if (info.meshCount == 0)
        {
            return;
        }

        const vk::DeviceSize indirectDrawCommandsBufferSize =
            static_cast<vk::DeviceSize>(info.meshCount) *
            sizeof(vk::DrawIndexedIndirectCommand);

        if (info.clearIndirectDrawCommandsBuffer)
        {
            cmdBuffer.fillBuffer(
                info.indirectDrawCommandsBuffer,
                0,
                indirectDrawCommandsBufferSize,
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
                .buffer = info.indirectDrawCommandsBuffer,
                .offset = 0,
                .size = indirectDrawCommandsBufferSize
            };

            cmdBuffer.pipelineBarrier2(
                vk::DependencyInfo{
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &clearToComputeBarrier
                });
        }

        if (info.drawableCount == 0)
        {
            return;
        }

        MeshInstancesCountPassData pushConstants{
            .indirectDrawCommandsBufferAddress = info.indirectDrawCommandsBufferAddress
        };

        cmdBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *pipeline);

        cmdBuffer.pushConstants(
            pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            PassSpecificDataOffset,
            sizeof(pushConstants),
            &pushConstants);

        const uint32_t groupCount = (info.drawableCount + workGroupSize - 1u) / workGroupSize;

        cmdBuffer.dispatch(
            groupCount,
            1,
            1);
    }
}