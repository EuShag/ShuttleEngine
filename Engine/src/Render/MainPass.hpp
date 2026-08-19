//
// Created by Shagu on 04.08.2026.
//

#ifndef SHUTTLEENGINE_MAINPASS_HPP
#define SHUTTLEENGINE_MAINPASS_HPP

#include "IncludeVulkan.hpp"
#include "Common.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include <glm/glm.hpp>

namespace shuttle::engine::render {

    enum class DebugMode : uint32_t {
        Final = 0,

        Albedo,
        Normal,
        Tangent,
        Bitangent,

        Metallic,
        Roughness,
        AmbientOcclusion,

        Emissive,

        UV,

        MeshId,
        MaterialId,
        InstanceId,

        ViewDepth,
        LinearDepth,

        WorldPosition,
        WorldNormal
    };

    struct alignas(16) MainPassSettings {
        // Resolution
        glm::vec2 renderResolution;
        glm::vec2 invRenderResolution;

        // Tonemapping
        float exposure = 0.03f;
        float gamma = 2.2f;

        // Image Based Lighting
        float diffuseIblStrength = 1.0f;
        float specularIblStrength = 1.0f;

        // Environment
        float skyboxIntensity = 1.0f;

        // Materials
        float emissiveIntensity = 1.0f;

        // Debug
        DebugMode debugModeOutput1{DebugMode::Final};
        DebugMode debugModeOutput2{DebugMode::Albedo};
        DebugMode debugModeOutput3{DebugMode::Normal};
        DebugMode debugModeOutput4{DebugMode::WorldPosition};

        // Reserved
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;


    };

    struct MainRenderPassDebugOutputsInfo {
        vk::ImageView debugOutput1Attachment{};
        vk::ImageView debugOutput2Attachment{};
        vk::ImageView debugOutput3Attachment{};
        vk::ImageView debugOutput4Attachment{};
    };

    struct MainRenderPassInfo {
        vk::DeviceAddress instanceRemapBufferAddress;
        vk::DeviceAddress mainPassSettingsBufferAddress;
        vk::DeviceAddress worldTransformBufferAddress;

        vk::ImageView colorAttachment;
        vk::ImageView depthAttachment;

        vk::Buffer indirectDrawCommandsBuffer;

        bool debugModeEnable = false;
        MainRenderPassDebugOutputsInfo debugOutputsInfo;

        uint32_t meshCount;
    };

    class MainRenderPass {
    public:
        [[nodiscard]] static vk::ResultValue<MainRenderPass> create(
            vk::Device device,
            vk::PipelineLayout pipelineLayout,
            vk::Format colorAttachmentInputFormat,
            vk::Format depthAttachmentInputFormat
        );

        void writeRenderCommands(
            vk::CommandBuffer cmd,
            MainRenderPassInfo const& mainRenderPassInfo,
            vk::Extent2D renderExtent
        );

        static constexpr AttachmentState colorAttachmentInput {
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
            .accessFlags = vk::AccessFlagBits2::eColorAttachmentWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };

        static constexpr AttachmentState colorAttachmentOutput {
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
            .accessFlags = vk::AccessFlagBits2::eColorAttachmentWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };

        static constexpr AttachmentState depthAttachmentInput {
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eEarlyFragmentTests
        };

        static constexpr AttachmentState depthAttachmentOutput {
            .layout = vk::ImageLayout::eDepthAttachmentOptimal,
            .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .stageFlags = vk::PipelineStageFlagBits2::eEarlyFragmentTests
        };

        static constexpr BufferState inputIndirectDrawCommandsBuffer {
            .accessFlags = vk::AccessFlagBits2::eIndirectCommandRead,
            .stageFlags = vk::PipelineStageFlagBits2::eDrawIndirect
        };

        static constexpr BufferState inputInstanceRemapBuffer {
            .accessFlags = vk::AccessFlagBits2::eShaderRead,
            .stageFlags = vk::PipelineStageFlagBits2::eVertexShader
        };

        static constexpr BufferState inputWorldTransformBuffer {
            .accessFlags = vk::AccessFlagBits2::eShaderRead,
            .stageFlags = vk::PipelineStageFlagBits2::eVertexShader
        };

        MainRenderPass() = default;
    private:

        MainRenderPass(
            vk::UniquePipeline mainPipeline,
            vk::UniquePipeline skyboxPipeline,
            vk::UniquePipeline debugMainPipeline,
            vk::UniquePipeline debugSkyboxPipeline,
            vk::PipelineLayout pipelineLayout) :
                mainPipeline(std::move(mainPipeline)),
                skyboxPipeline(std::move(skyboxPipeline)),
                debugMainPipeline(std::move(debugMainPipeline)),
                debugSkyboxPipeline(std::move(debugSkyboxPipeline)),
                pipelineLayout(pipelineLayout) {}

        vk::UniquePipeline mainPipeline;
        vk::UniquePipeline skyboxPipeline;

        vk::UniquePipeline debugMainPipeline;
        vk::UniquePipeline debugSkyboxPipeline;

        vk::PipelineLayout pipelineLayout;
    };

    class MainPassSettingSystem {
    public:
        [[nodiscard]] static vk::ResultValue<MainPassSettingSystem> create(
            vk::Device device,
            resources::DeviceAllocator const& allocator);

        void updateSettings(MainPassSettings const& settings) const;

        [[nodiscard]] vk::DeviceAddress getMainPassSettingsBufferAddress() const {
            return mainPassSettingsBufferAddress;
        }

        MainPassSettingSystem() = default;
        MainPassSettingSystem(
            resources::UniqueAllocatedBuffer mainPassSettingsBuffer,
            vk::DeviceAddress mainPassSettingsBufferAddress,
            void* mainPassSettingsMappedPtr
        ) : mainPassSettingsBuffer(std::move(mainPassSettingsBuffer)),
            mainPassSettingsBufferAddress(mainPassSettingsBufferAddress),
            mainPassSettingsMappedPtr(mainPassSettingsMappedPtr) {}

    private:

        resources::UniqueAllocatedBuffer mainPassSettingsBuffer;
        vk::DeviceAddress mainPassSettingsBufferAddress{};
        void* mainPassSettingsMappedPtr{};
    };
}
#endif //SHUTTLEENGINE_MAINPASS_HPP
