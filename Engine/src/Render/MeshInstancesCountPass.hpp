//
// Created by Shagu on 05.08.2026.
//

#ifndef SHUTTLEENGINE_MESHINSTANCESCOUNTPASS_HPP
#define SHUTTLEENGINE_MESHINSTANCESCOUNTPASS_HPP

#include "IncludeVulkan.hpp"
#include "Common.hpp"

namespace shuttle::engine::render
{
    struct MeshInstancesCountPassInfo
    {
        vk::DeviceAddress indirectDrawCommandsBufferAddress{};

        vk::Buffer indirectDrawCommandsBuffer{};

        uint32_t drawableCount{};
        uint32_t meshCount{};

        bool clearIndirectDrawCommandsBuffer = true;
    };

    class MeshInstancesCountPass
    {
    public:
        [[nodiscard]]
        static vk::ResultValue<MeshInstancesCountPass> create(
            vk::Device device,
            vk::PipelineLayout pipelineLayout);

        void writeRenderCommands(
            vk::CommandBuffer cmdBuffer,
            MeshInstancesCountPassInfo const& info) const;

        static constexpr uint32_t workGroupSize = 64u;

        // ============================================================
        // Output Buffers
        // ============================================================

        static constexpr BufferState clearIndirectDrawCommandsBuffer{
            .accessFlags = vk::AccessFlagBits2::eTransferWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eTransfer
        };

        static constexpr BufferState outputIndirectDrawCommandsBuffer{
            .accessFlags =
                vk::AccessFlagBits2::eShaderRead |
                vk::AccessFlagBits2::eShaderWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        MeshInstancesCountPass() = default;

    private:

        MeshInstancesCountPass(
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

#endif // SHUTTLEENGINE_MESHINSTANCESCOUNTPASS_HPP