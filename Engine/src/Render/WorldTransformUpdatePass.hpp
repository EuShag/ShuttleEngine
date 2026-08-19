//
// Created by Shagu on 05.08.2026.
//

#ifndef SHUTTLEENGINE_WORLDTRANSFORMUPDATEPASS_HPP
#define SHUTTLEENGINE_WORLDTRANSFORMUPDATEPASS_HPP

#include "IncludeVulkan.hpp"
#include "Common.hpp"
#include <Assets/Formats/Scene.hpp>

namespace shuttle::engine::render {

    struct WorldTransformUpdatePassInfo {
        vk::DeviceAddress worldTransformBufferAddress;

        vk::Buffer worldTransformBuffer;

        std::span<const assets::formats::scene::NodeLevelRange> nodeLevelRanges;
    };

    class WorldTransformUpdatePass {
    public:
        [[nodiscard]] static vk::ResultValue<WorldTransformUpdatePass> create(vk::Device device, vk::PipelineLayout pipelineLayout);

        void writeRenderCommands(vk::CommandBuffer cmdBuffer, WorldTransformUpdatePassInfo const& info) const;
        
        static constexpr BufferState outputWorldTransformBuffer{
            .accessFlags = vk::AccessFlagBits2::eShaderWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eComputeShader
        };

        WorldTransformUpdatePass() = default;
    private:
        WorldTransformUpdatePass(
            vk::UniquePipeline pipeline,
            vk::PipelineLayout pipelineLayout) :
                pipeline(std::move(pipeline)),
                pipelineLayout(pipelineLayout) {}

        vk::UniquePipeline pipeline;
        vk::PipelineLayout pipelineLayout;

        static constexpr uint32_t WorkGroupSize = 64u;
    };
}

#endif //SHUTTLEENGINE_WORLDTRANSFORMUPDATEPASS_HPP
