//
// Created by Shagu on 05.08.2026.
//

#include "PrefixSumPass.hpp"

#include <filesystem>
#include <fstream>

#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine::render
{
    namespace
    {
        struct alignas(16) PrefixSumPassData
        {
            vk::DeviceAddress indirectDrawCommandsBufferAddress{};

            uint32_t padding0{};
            uint32_t padding1{};
        };

        vk::ResultValue<vk::UniquePipeline> createPrefixSumPipeline(
            vk::Device device,
            vk::PipelineLayout pipelineLayout)
        {
            auto [createShaderModuleResult, shaderModule] =
                loadAndCreateShaderModuleUnique( device, "../shaders/prefixSumPass.comp.spv");

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

    vk::ResultValue<PrefixSumPass> PrefixSumPass::create(
        vk::Device device,
        vk::PipelineLayout pipelineLayout)
    {
        auto [createPipelineResult, pipeline] =
            createPrefixSumPipeline(
                device,
                pipelineLayout);

        if (createPipelineResult != vk::Result::eSuccess)
        {
            return {createPipelineResult, {}};
        }

        return {
            vk::Result::eSuccess,
            PrefixSumPass{
                std::move(pipeline),
                pipelineLayout
            }
        };
    }

    void PrefixSumPass::writeRenderCommands(
        vk::CommandBuffer cmdBuffer,
        PrefixSumPassInfo const& info) const
    {

        PrefixSumPassData pushConstants{
            .indirectDrawCommandsBufferAddress = info.indirectDrawCommandsBufferAddress,
            .padding0 = 0,
            .padding1 = 0
        };

        cmdBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *pipeline);

        cmdBuffer.pushConstants(
            pipelineLayout,
             vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            PassSpecificDataOffset,
            sizeof(PrefixSumPassData),
            &pushConstants);

        cmdBuffer.dispatch(1, 1, 1);
    }
}
