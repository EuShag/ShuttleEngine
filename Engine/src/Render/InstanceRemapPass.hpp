//
// Created by Shagu on 05.08.2026.
//

#ifndef SHUTTLEENGINE_INSTANCEREMAPPASS_HPP
#define SHUTTLEENGINE_INSTANCEREMAPPASS_HPP

#include "IncludeVulkan.hpp"
#include "Common.hpp"

namespace shuttle::engine::render
{
    struct InstanceRemapPassInfo
    {
        vk::DeviceAddress indirectDrawCommandsBufferAddress{};
        vk::DeviceAddress instanceRemapBufferAddress{};
        vk::DeviceAddress meshInstanceCursorBufferAddress{};

        vk::Buffer meshInstanceCursorBuffer{};

        uint32_t drawableCount{};
        uint32_t meshCount{};

        bool clearMeshInstanceCursorBuffer = true;
    };

    class InstanceRemapPass
    {
    public:
        [[nodiscard]]
        static vk::ResultValue<InstanceRemapPass> create(
            vk::Device device,
            vk::PipelineLayout pipelineLayout);

        void writeRenderCommands(
            vk::CommandBuffer cmdBuffer,
            InstanceRemapPassInfo const& info) const;

        static constexpr uint32_t WorkGroupSize = 64u;

        // ============================================================
        // Input Buffers
        // ============================================================

        static constexpr BufferState inputSceneDrawablesBuffer{
            .accessFlags = vk::AccessFlagBits2::eShaderRead,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        static constexpr BufferState inputIndirectDrawCommandsBuffer{
            .accessFlags = vk::AccessFlagBits2::eShaderRead,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        // ============================================================
        // Output Buffers
        // ============================================================

        static constexpr BufferState clearMeshInstanceCursorBuffer{
            .accessFlags = vk::AccessFlagBits2::eTransferWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eTransfer
        };

        static constexpr BufferState outputMeshInstanceCursorBuffer{
            .accessFlags =
                vk::AccessFlagBits2::eShaderRead |
                vk::AccessFlagBits2::eShaderWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        static constexpr BufferState outputInstanceRemapBuffer{
            .accessFlags = vk::AccessFlagBits2::eShaderWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        InstanceRemapPass() = default;
    private:

        InstanceRemapPass(
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

#endif // SHUTTLEENGINE_INSTANCEREMAPPASS_HPP