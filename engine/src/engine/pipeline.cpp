//
// Created by Shagu on 12.09.2026.
//
#include "engine_impl.hpp"

namespace shuttle::engine {

    vk::ResultValue<vk::UniquePipeline> EngineImpl::createComputePipeline(std::filesystem::path const &shaderPath) {
        auto [createShaderModuleResult, shaderModule] =
                createShaderModuleUnique(shaderPath);

        if (createShaderModuleResult != vk::Result::eSuccess)
        {
            return {createShaderModuleResult, {}};
        }

        vk::PipelineShaderStageCreateInfo stageInfo{
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = *shaderModule,
            .pName = "main"
        };

        return m_uniqueDevice->createComputePipelineUnique(
            {},
            {
                .stage = stageInfo,
                .layout = *m_pipelineLayout
            }, nullptr, m_dispatcher);
    }

    vk::ResultValue<vk::UniquePipeline> EngineImpl::createMainRenderPipeline(
        vk::Format colorFormat,
        vk::Format depthFormat,
        bool enableDebug = false) const
    {
        vk::UniqueShaderModule vertexShaderModule;
        vk::UniqueShaderModule fragmentShaderModule;
        if (enableDebug){
            auto [createVertexShaderModuleResult, vertexShaderModule_] = createShaderModuleUnique("../shaders/main_pass_debug.vert.spv");
            auto [createFragmentShaderModuleResult, fragmentShaderModule_] = createShaderModuleUnique("../shaders/main_pass_debug.frag.spv");
            if (createVertexShaderModuleResult != vk::Result::eSuccess)
            { return {createVertexShaderModuleResult, {}}; }

            if (createFragmentShaderModuleResult != vk::Result::eSuccess)
            { return {createFragmentShaderModuleResult, {}}; }
            vertexShaderModule = std::move(vertexShaderModule_);
            fragmentShaderModule = std::move(fragmentShaderModule_);
        }
        else {
            auto [createVertexShaderModuleResult, vertexShaderModule_] = createShaderModuleUnique("../shaders/main_pass.vert.spv");
            auto [createFragmentShaderModuleResult, fragmentShaderModule_] = createShaderModuleUnique("../shaders/main_pass.frag.spv");
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

        return m_uniqueDevice->createGraphicsPipelineUnique({}, {
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
            .layout = *m_pipelineLayout,
            .renderPass = nullptr, // Using dynamic rendering
            .subpass = 0
        }, nullptr, m_dispatcher);
    }

    [[nodiscard]] vk::ResultValue<vk::UniquePipeline> EngineImpl::createSkyboxPipeline(
        vk::Format colorFormat,
        vk::Format depthFormat,
        bool enableDebug) const {

        vk::UniqueShaderModule vertexShader{};
        vk::UniqueShaderModule fragmentShader{};

        if (enableDebug) {
            auto [createVertexShaderModuleResult, vertexShaderModule] = createShaderModuleUnique("../shaders/skybox_debug.vert.spv");
            auto [createFragmentShaderModuleResult, fragmentShaderModule] = createShaderModuleUnique("../shaders/skybox_debug.frag.spv");
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
            auto [createVertexShaderModuleResult, vertexShaderModule] = createShaderModuleUnique("../shaders/skybox.vert.spv");
            auto [createFragmentShaderModuleResult, fragmentShaderModule] = createShaderModuleUnique("../shaders/skybox.frag.spv");
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

        return m_uniqueDevice->createGraphicsPipelineUnique({}, {
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
            .layout = *m_pipelineLayout,
            .renderPass = nullptr, // Using dynamic rendering
            .subpass = 0
        }, nullptr, m_dispatcher);
    }
}
