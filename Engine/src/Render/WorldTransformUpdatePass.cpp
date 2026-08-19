//
// Created by Shagu on 05.08.2026.
//

#include "WorldTransformUpdatePass.hpp"

#include <fstream>
#include <filesystem>

#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine::render {
    namespace {
        struct alignas(16) WorldTransformUpdatePassData {
            vk::DeviceAddress worldTransformBufferAddress{};
            uint32_t firstNode{};
            uint32_t nodeCount{};

            uint32_t reserved0{};
            uint32_t reserved1{};
        };

        vk::ResultValue<vk::UniquePipeline> createWorldTransformUpdatePipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
        {

            auto [createShaderModuleResult, uniqueShaderModule] = loadAndCreateShaderModuleUnique(device, "../shaders/worldTransformUpdate.comp.spv");
            if (createShaderModuleResult != vk::Result::eSuccess)
            {
                return {createShaderModuleResult, {}};
            }

            vk::PipelineShaderStageCreateInfo stageInfo{
                .stage = vk::ShaderStageFlagBits::eCompute,
                .module = *uniqueShaderModule,
                .pName = "main"
            };

            vk::ComputePipelineCreateInfo pipelineInfo{
                .stage = stageInfo,
                .layout = pipelineLayout
            };

            return device.createComputePipelineUnique({}, pipelineInfo);
        }
    }


    vk::ResultValue<WorldTransformUpdatePass> WorldTransformUpdatePass::create(
        vk::Device device,
        vk::PipelineLayout pipelineLayout) {

        auto [createPipelineResult, pipeline] = createWorldTransformUpdatePipeline(device, pipelineLayout);
        if (createPipelineResult != vk::Result::eSuccess) return {createPipelineResult, {}};

        return {
            vk::Result::eSuccess,
            WorldTransformUpdatePass {
                std::move(pipeline),
                pipelineLayout
            }
        };
    }

    void WorldTransformUpdatePass::writeRenderCommands(
        vk::CommandBuffer cmdBuffer,
        WorldTransformUpdatePassInfo const& info) const
    {
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);

        for (size_t i = 0; i < info.nodeLevelRanges.size(); ++i)
        {
            auto const& nodeLevelRange =
                info.nodeLevelRanges[i];

            if (nodeLevelRange.count == 0) continue;

            WorldTransformUpdatePassData pushConstants{
                .worldTransformBufferAddress = info.worldTransformBufferAddress,
                .firstNode = nodeLevelRange.startIndex,
                .nodeCount = nodeLevelRange.count,
                .reserved0 = 0,
                .reserved1 = 0
            };

            cmdBuffer.pushConstants(
                pipelineLayout,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
                PassSpecificDataOffset,
                sizeof(pushConstants),
                &pushConstants);

            uint32_t const groupCount = (nodeLevelRange.count + WorkGroupSize - 1) / WorkGroupSize;

            cmdBuffer.dispatch(groupCount, 1, 1);

            // После последнего уровня барьер не нужен.
            if (i + 1 == info.nodeLevelRanges.size()) continue;

            vk::BufferMemoryBarrier2 barrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
                .buffer = info.worldTransformBuffer,
                .offset = 0,
                .size = vk::WholeSize
            };

            cmdBuffer.pipelineBarrier2( vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &barrier
            });
        }
    }
}
