//
// Created by Shagu on 04.08.2026.
//

#include "MainPass.hpp"

#include "Render.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine::render {

    namespace {

        struct alignas(16) MainPassData {
            vk::DeviceAddress mainPassSettingsAddress;
            vk::DeviceAddress instanceRemapAddress;
            vk::DeviceAddress worldTransformBufferAddress;
            uint64_t reserved;
        };

        vk::ResultValue<vk::UniquePipeline> createMainRenderPipeline(
            vk::Device device,
            vk::PipelineLayout pipelineLayout,
            vk::Format colorFormat,
            vk::Format depthFormat,
            bool enableDebug = false)
        {
            vk::UniqueShaderModule vertexShaderModule;
            vk::UniqueShaderModule fragmentShaderModule;
            if (enableDebug){
                auto [createVertexShaderModuleResult, vertexShaderModule_] = loadAndCreateShaderModuleUnique(device, "../shaders/main_pass_debug.vert.spv");
                auto [createFragmentShaderModuleResult, fragmentShaderModule_] = loadAndCreateShaderModuleUnique(device, "../shaders/main_pass_debug.frag.spv");
                if (createVertexShaderModuleResult != vk::Result::eSuccess)
                { return {createVertexShaderModuleResult, {}}; }

                if (createFragmentShaderModuleResult != vk::Result::eSuccess)
                { return {createFragmentShaderModuleResult, {}}; }
                vertexShaderModule = std::move(vertexShaderModule_);
                fragmentShaderModule = std::move(fragmentShaderModule_);
            }
            else {
                auto [createVertexShaderModuleResult, vertexShaderModule_] = loadAndCreateShaderModuleUnique(device, "../shaders/main_pass.vert.spv");
                auto [createFragmentShaderModuleResult, fragmentShaderModule_] = loadAndCreateShaderModuleUnique(device, "../shaders/main_pass.frag.spv");
                if (createVertexShaderModuleResult != vk::Result::eSuccess)
                { return {createVertexShaderModuleResult, {}}; }

                if (createFragmentShaderModuleResult != vk::Result::eSuccess)
                { return {createFragmentShaderModuleResult, {}}; }
                vertexShaderModule = std::move(vertexShaderModule_);
                fragmentShaderModule = std::move(fragmentShaderModule_);
            }

            std::array shaderStages = {
                vk::PipelineShaderStageCreateInfo{
                    .stage = vk::ShaderStageFlagBits::eVertex,
                    .module = *vertexShaderModule,
                    .pName = "main"},
                vk::PipelineShaderStageCreateInfo{
                    .stage = vk::ShaderStageFlagBits::eFragment,
                    .module = *fragmentShaderModule,
                    .pName = "main"
                }
            };

            vk::PipelineVertexInputStateCreateInfo vertexInput{};

            vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
                .topology = vk::PrimitiveTopology::eTriangleList
            };

            vk::PipelineViewportStateCreateInfo viewportState{
                .viewportCount = 1,
                .pViewports = nullptr,
                .scissorCount = 1,
                .pScissors = nullptr
            };

            vk::PipelineRasterizationStateCreateInfo rasterizer{
                .depthClampEnable = vk::False,
                .rasterizerDiscardEnable = vk::False,
                .polygonMode = vk::PolygonMode::eFill,
                .cullMode = vk::CullModeFlagBits::eBack,
                .frontFace = vk::FrontFace::eCounterClockwise,
                .lineWidth = 1.0f
            };

            vk::PipelineMultisampleStateCreateInfo multisample{
                .rasterizationSamples = vk::SampleCountFlagBits::e1,
                .sampleShadingEnable = vk::False
            };

            vk::PipelineDepthStencilStateCreateInfo depthStencil{
                .depthTestEnable = vk::True,
                .depthWriteEnable = vk::True,
                .depthCompareOp = vk::CompareOp::eLessOrEqual
            };

            vk::PipelineColorBlendAttachmentState blendAttachment{
                .blendEnable = vk::False,
                .colorWriteMask =
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA
            };

            std::array blendAttachments {
                blendAttachment,
                blendAttachment,
                blendAttachment,
                blendAttachment,
                blendAttachment
            };

            vk::PipelineColorBlendStateCreateInfo colorBlend{
                .logicOpEnable = vk::False,
                .attachmentCount = enableDebug ? 5u : 1u,
                .pAttachments = blendAttachments.data()
            };

            std::array dynamicStates{
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor
            };

            vk::PipelineDynamicStateCreateInfo dynamicState{
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()
            };

            std::array colorFormats {
                colorFormat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat
            };

            vk::PipelineRenderingCreateInfo renderingInfo{
                .colorAttachmentCount = enableDebug ? 5u : 1u,
                .pColorAttachmentFormats = colorFormats.data(),
                .depthAttachmentFormat = depthFormat
            };

            return device.createGraphicsPipelineUnique({}, {
                .pNext = &renderingInfo,
                .stageCount = static_cast<uint32_t>(shaderStages.size()),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlend,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = nullptr, // Using dynamic rendering
                .subpass = 0
            });
        }

        vk::ResultValue<vk::UniquePipeline> createSkyboxPipeline(
            vk::Device device,
            vk::PipelineLayout pipelineLayout,
            vk::Format colorFormat,
            vk::Format depthFormat,
            bool enableDebug = false)
        {

            vk::UniqueShaderModule vertexShader{};
            vk::UniqueShaderModule fragmentShader{};

            if (enableDebug) {
                auto [createVertexShaderModuleResult, vertexShaderModule] = loadAndCreateShaderModuleUnique(device, "../shaders/skybox_debug.vert.spv");
                auto [createFragmentShaderModuleResult, fragmentShaderModule] = loadAndCreateShaderModuleUnique(device, "../shaders/skybox_debug.frag.spv");
                if (createVertexShaderModuleResult != vk::Result::eSuccess) {
                    return {createVertexShaderModuleResult, {}};
                }

                if (createFragmentShaderModuleResult != vk::Result::eSuccess) {
                    return {createFragmentShaderModuleResult, {}};
                }

                vertexShader = std::move(vertexShaderModule);
                fragmentShader = std::move(fragmentShaderModule);
            }
            else {
                auto [createVertexShaderModuleResult, vertexShaderModule] = loadAndCreateShaderModuleUnique(device, "../shaders/skybox.vert.spv");
                auto [createFragmentShaderModuleResult, fragmentShaderModule] = loadAndCreateShaderModuleUnique(device, "../shaders/skybox.frag.spv");
                if (createVertexShaderModuleResult != vk::Result::eSuccess) {
                    return {createVertexShaderModuleResult, {}};
                }

                if (createFragmentShaderModuleResult != vk::Result::eSuccess) {
                    return {createFragmentShaderModuleResult, {}};
                }

                vertexShader = std::move(vertexShaderModule);
                fragmentShader = std::move(fragmentShaderModule);
            }

            std::array shaderStages = {
                vk::PipelineShaderStageCreateInfo{
                    .stage = vk::ShaderStageFlagBits::eVertex,
                    .module = *vertexShader,
                    .pName = "main"
                },
                vk::PipelineShaderStageCreateInfo{
                    .stage = vk::ShaderStageFlagBits::eFragment,
                    .module = *fragmentShader,
                    .pName = "main"
                }
            };

            vk::PipelineVertexInputStateCreateInfo vertexInput{};

            vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
                .topology = vk::PrimitiveTopology::eTriangleList
            };

            vk::PipelineViewportStateCreateInfo viewportState{
                .viewportCount = 1,
                .pViewports = nullptr,
                .scissorCount = 1,
                .pScissors = nullptr
            };

            vk::PipelineRasterizationStateCreateInfo rasterizer{
                .depthClampEnable = vk::False,
                .rasterizerDiscardEnable = vk::False,
                .polygonMode = vk::PolygonMode::eFill,
                .cullMode = vk::CullModeFlagBits::eNone,
                .frontFace = vk::FrontFace::eCounterClockwise,
                .lineWidth = 1.0f
            };

            vk::PipelineMultisampleStateCreateInfo multisample{
                .rasterizationSamples = vk::SampleCountFlagBits::e1,
                .sampleShadingEnable = vk::False
            };

            vk::PipelineDepthStencilStateCreateInfo depthStencil{
                .depthTestEnable = vk::True,
                .depthWriteEnable = vk::False,
                .depthCompareOp = vk::CompareOp::eLessOrEqual
            };

            vk::PipelineColorBlendAttachmentState blendAttachment{
                .blendEnable = vk::False,
                .colorWriteMask =
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA
            };

            std::array blendAttachments {
                blendAttachment,
                blendAttachment,
                blendAttachment,
                blendAttachment,
                blendAttachment
            };

            vk::PipelineColorBlendStateCreateInfo colorBlend{
                .logicOpEnable = vk::False,
                .attachmentCount = enableDebug ? 5u : 1u,
                .pAttachments = blendAttachments.data()
            };

            std::array dynamicStates{
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor
            };

            vk::PipelineDynamicStateCreateInfo dynamicState{
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()
            };

            std::array colorFormats {
                colorFormat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat
            };

            vk::PipelineRenderingCreateInfo renderingInfo{
                .colorAttachmentCount = enableDebug ? 5u : 1u,
                .pColorAttachmentFormats = colorFormats.data(),
                .depthAttachmentFormat = depthFormat
            };

            return device.createGraphicsPipelineUnique({}, {
                .pNext = &renderingInfo,
                .stageCount = static_cast<uint32_t>(shaderStages.size()),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlend,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = nullptr, // Using dynamic rendering
                .subpass = 0
            });
        }
    }

    vk::ResultValue<MainRenderPass> MainRenderPass::create(
        vk::Device device,
        vk::PipelineLayout pipelineLayout,
        vk::Format colorAttachmentInputFormat,
        vk::Format depthAttachmentInputFormat) {
        auto [createMainPipelineResult, mainPipeline] = createMainRenderPipeline(
            device, pipelineLayout, colorAttachmentInputFormat, depthAttachmentInputFormat);

        if (createMainPipelineResult != vk::Result::eSuccess) {
            return {createMainPipelineResult, {}};
        }

        auto [createSkyboxPipelineResult, skyboxPipeline] = createSkyboxPipeline(
            device, pipelineLayout, colorAttachmentInputFormat, depthAttachmentInputFormat);

        if (createSkyboxPipelineResult != vk::Result::eSuccess) {
            return {createSkyboxPipelineResult, {}};
        }

        auto [createDebugMainPipelineResult, debugMainPipeline] = createMainRenderPipeline(
            device, pipelineLayout, colorAttachmentInputFormat, depthAttachmentInputFormat, true);
        if (createDebugMainPipelineResult != vk::Result::eSuccess) {
            return {createDebugMainPipelineResult, {}};
        }

        auto [createDebugSkyboxPipelineResult, debugSkyboxPipeline] = createSkyboxPipeline(
            device, pipelineLayout, colorAttachmentInputFormat, depthAttachmentInputFormat, true);

        if (createDebugSkyboxPipelineResult != vk::Result::eSuccess) {
            return {createDebugSkyboxPipelineResult, {}};
        }

        return {vk::Result::eSuccess, MainRenderPass{
            std::move(mainPipeline),
            std::move(skyboxPipeline),
            std::move(debugMainPipeline),
            std::move(debugSkyboxPipeline), pipelineLayout
        }};
    }

    void MainRenderPass::writeRenderCommands(
        vk::CommandBuffer const cmd,
        MainRenderPassInfo const& mainRenderPassInfo,
        vk::Extent2D const renderExtent) {

        auto const currentMainPipeline = mainRenderPassInfo.debugModeEnable ? debugMainPipeline.get() : mainPipeline.get();
        auto const currentSkyboxPipeline = mainRenderPassInfo.debugModeEnable ? debugSkyboxPipeline.get() : skyboxPipeline.get();

        std::array const colorAttachments {
            vk::RenderingAttachmentInfo {
                .imageView = mainRenderPassInfo.colorAttachment,
                .imageLayout = colorAttachmentInput.layout,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{.color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}}
            },
            vk::RenderingAttachmentInfo {
                .imageView = mainRenderPassInfo.debugOutputsInfo.debugOutput1Attachment,
                .imageLayout = colorAttachmentInput.layout,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
                }
            },
            vk::RenderingAttachmentInfo {
                .imageView = mainRenderPassInfo.debugOutputsInfo.debugOutput2Attachment,
                .imageLayout = colorAttachmentInput.layout,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
                }
            },
            vk::RenderingAttachmentInfo {
                .imageView = mainRenderPassInfo.debugOutputsInfo.debugOutput3Attachment,
                .imageLayout = colorAttachmentInput.layout,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
                }
            },
            vk::RenderingAttachmentInfo {
                .imageView = mainRenderPassInfo.debugOutputsInfo.debugOutput4Attachment,
                .imageLayout = colorAttachmentInput.layout,
                .resolveMode = vk::ResolveModeFlagBits::eNone,
                .resolveImageView = nullptr,
                .resolveImageLayout = vk::ImageLayout::eUndefined,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
                }
            }
        };

        vk::RenderingAttachmentInfo const depthAttachment {
            .imageView = mainRenderPassInfo.depthAttachment,
            .imageLayout = depthAttachmentInput.layout,
            .resolveMode = vk::ResolveModeFlagBits::eNone,
            .resolveImageView = nullptr,
            .resolveImageLayout = vk::ImageLayout::eUndefined,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0}}
        };

        MainPassData mainPassData{
            .mainPassSettingsAddress = mainRenderPassInfo.mainPassSettingsBufferAddress,
            .instanceRemapAddress = mainRenderPassInfo.instanceRemapBufferAddress,
            .worldTransformBufferAddress = mainRenderPassInfo.worldTransformBufferAddress,
            .reserved = 0,
        };

        cmd.pushConstants(
            pipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            PassSpecificDataOffset, sizeof(MainPassData),
            &mainPassData
        );

        cmd.beginRendering(
            vk::RenderingInfo{
                .renderArea = vk::Rect2D{
                    .offset = vk::Offset2D{.x = 0, .y = 0},
                    .extent = renderExtent
                },
                .layerCount = 1,
                .colorAttachmentCount = mainRenderPassInfo.debugModeEnable ? static_cast<uint32_t>(colorAttachments.size()) : 1,
                .pColorAttachments = colorAttachments.data(),
                .pDepthAttachment = &depthAttachment
            }
        );

        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(renderExtent.width),
            .height = static_cast<float>(renderExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vk::Rect2D const scissor{
            .offset = vk::Offset2D{.x = 0, .y = 0},
            .extent = renderExtent
        };

        cmd.setViewport(0, 1, &viewport);
        cmd.setScissor(0, 1, &scissor);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            currentMainPipeline);

        cmd.drawIndexedIndirect(
            mainRenderPassInfo.indirectDrawCommandsBuffer,
            0,
            mainRenderPassInfo.meshCount,
            sizeof(vk::DrawIndexedIndirectCommand));

        cmd.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            currentSkyboxPipeline);

        cmd.draw(36, 1, 0, 0);

        cmd.endRendering();
    }

    void MainPassSettingSystem::updateSettings(MainPassSettings const &settings) const {
        std::memcpy(mainPassSettingsMappedPtr, &settings, sizeof(MainPassSettings));
    }

    vk::ResultValue<MainPassSettingSystem> MainPassSettingSystem::create(
        vk::Device device,
        resources::DeviceAllocator const& allocator
    ){
        auto [createMainPassSettingsBufferResult, mainPassSettingsBuffer] = allocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = sizeof(MainPassSettings),
                .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
                .sharingMode = vk::SharingMode::eExclusive
            },
            resources::MemoryUsage::eCpuToGpu,
            static_cast<resources::AllocationCreateFlagBits>(
                static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped) |
                static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite))
        );
        if (createMainPassSettingsBufferResult != vk::Result::eSuccess) {
            return {createMainPassSettingsBufferResult, {}};
        }

        vk::DeviceAddress const mainPassSettingsBufferAddress = device.getBufferAddress({.buffer = *mainPassSettingsBuffer});
        void* const mainPassSettingsMappedPtr = allocator.getMappedPointer(*mainPassSettingsBuffer);

        if (mainPassSettingsMappedPtr == nullptr) {
            return {vk::Result::eErrorMemoryMapFailed, {}};
        }
        return {vk::Result::eSuccess, MainPassSettingSystem{
            std::move(mainPassSettingsBuffer),
            mainPassSettingsBufferAddress,
            mainPassSettingsMappedPtr
        }};
    }
}
