//
// Created by Shagu on 05.08.2026.
//

#ifndef SHUTTLEENGINE_PREFIXSUMPASS_HPP
#define SHUTTLEENGINE_PREFIXSUMPASS_HPP

#include "IncludeVulkan.hpp"
#include "Common.hpp"

namespace shuttle::engine::render
{
    struct PrefixSumPassInfo
    {
        vk::DeviceAddress indirectDrawCommandsBufferAddress{};

        vk::Buffer indirectDrawCommandsBuffer{};
    };

    class PrefixSumPass
    {
    public:
        [[nodiscard]]
        static vk::ResultValue<PrefixSumPass> create(
            vk::Device device,
            vk::PipelineLayout pipelineLayout);

        void writeRenderCommands(
            vk::CommandBuffer cmdBuffer,
            PrefixSumPassInfo const& info) const;

        static constexpr BufferState inputIndirectDrawCommandsBuffer {
            .accessFlags = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        // ============================================================
        // Output Buffers
        // ============================================================

        static constexpr BufferState outputIndirectDrawCommandsBuffer{
            .accessFlags = vk::AccessFlagBits2::eShaderWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        PrefixSumPass() = default;
    private:

        PrefixSumPass(
            vk::UniquePipeline pipeline,
            vk::PipelineLayout pipelineLayout)
            :
            pipeline(std::move(pipeline)),
            pipelineLayout(pipelineLayout)
        {
        }

        vk::UniquePipeline pipeline{};
        vk::PipelineLayout pipelineLayout{};
    };
}

#endif // SHUTTLEENGINE_PREFIXSUMPASS_HPP