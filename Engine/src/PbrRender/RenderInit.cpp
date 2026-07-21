//
// Created by Shagu on 29.05.2026.
//
#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine {

        vk::Result PbrRender::initMainRenderPass(vk::Device device, vk::ImageLayout finalLayout) {

        std::array attachmentDescriptions {
            vk::AttachmentDescription{
                .format = vk::Format::eB8G8R8A8Srgb,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = finalLayout
            },
            vk::AttachmentDescription{
                .format = vk::Format::eD32SfloatS8Uint,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eDontCare,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            }
        };

        std::array attachmentReferences {
            vk::AttachmentReference{
                .attachment = 0,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference{
                .attachment = 1,
                .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            }
        };

        vk::SubpassDescription subpassDescription {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReferences[0],
            .pDepthStencilAttachment = &attachmentReferences[1]
        };

        std::array subpassDependencies {
            vk::SubpassDependency{
                .srcSubpass = vk::SubpassExternal,
                .dstSubpass = 0,
                .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = {},
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            },
            vk::SubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = vk::SubpassExternal,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = {},
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            }
        };

        vk::RenderPassCreateInfo const renderPassCreateInfo {
            .attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
            .pDependencies = subpassDependencies.data()
        };

        auto mainRenderPassResultValue = device.createRenderPassUnique(renderPassCreateInfo);
        if (mainRenderPassResultValue.result != vk::Result::eSuccess) {
            return mainRenderPassResultValue.result;
        }
        mainRenderPass = std::move(mainRenderPassResultValue.value);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initShadowRenderPass(vk::Device device) {
        vk::AttachmentDescription attachmentDescription {
            .format = vk::Format::eD32SfloatS8Uint,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::AttachmentReference attachmentReference {
            .attachment = 0,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        vk::SubpassDescription subpassDescription {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 0,
            .pDepthStencilAttachment = &attachmentReference
        };

        // Зависимости отличные! Они синхронизируют запись глубины и последующее чтение.
        std::array subpassDependencies {
            vk::SubpassDependency{
                .srcSubpass = vk::SubpassExternal,
                .dstSubpass = 0,
                // Смена: теперь мы ждем начала прохода, а не фрагментного шейдера
                .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            },
            vk::SubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = vk::SubpassExternal,
                .srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
                .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
                .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            }
        };

        vk::RenderPassCreateInfo const renderPassCreateInfo {
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
            .pDependencies = subpassDependencies.data()
        };

        auto shadowRenderPassResultValue = device.createRenderPassUnique(renderPassCreateInfo);
        if (shadowRenderPassResultValue.result != vk::Result::eSuccess) {
            return shadowRenderPassResultValue.result;
        }
        shadowRenderPass = std::move(shadowRenderPassResultValue.value);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initMainPipeline(vk::Device device) {
        // 1. Загрузка шейдеров
        auto vertModule = loadAndCreateShaderModuleUnique(device, "../shaders/pbr.vert.spv");
        auto fragModule = loadAndCreateShaderModuleUnique(device, "../shaders/pbr.frag.spv");

        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertModule.value,
                .pName = "main"
            },
            {
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *fragModule.value,
                .pName = "main"
            }
        };

        // 2. Описание формата вершин (Вершинный Буфер)
        std::array vertexBindingDescriptions {
            // For positions only
            vk::VertexInputBindingDescription{
                .binding = 0,
                .stride = sizeof(PositionAttribute),
                .inputRate = vk::VertexInputRate::eVertex
            },
            // For normals, tangents, uvs
            vk::VertexInputBindingDescription{
                .binding = 1,
                .stride = sizeof(NormalTangentUvAttribute), // Шаг равен размеру всей структуры вершины
                .inputRate = vk::VertexInputRate::eVertex
            }
        };

        std::array vertexAttributeDescriptions {
            vk::VertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = 0
            },
            vk::VertexInputAttributeDescription{
                .location = 1,
                .binding = 1,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(NormalTangentUvAttribute, normal)
            },
            vk::VertexInputAttributeDescription{
                .location = 2,
                .binding = 1,
                .format = vk::Format::eR32G32Sfloat,
                .offset = offsetof(NormalTangentUvAttribute, uv)
            },
            vk::VertexInputAttributeDescription{
                .location = 3,
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = offsetof(NormalTangentUvAttribute, tangent)
            }
        };

        vk::PipelineVertexInputStateCreateInfo vertexInput {
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size()),
            .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
            .pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
        };

        // 3. Сборка геометрии (треугольники)
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };

        // 4. Настройка Viewport и Scissor (делаем динамическими, как и в тенях)
        vk::PipelineViewportStateCreateInfo viewportState {
            .viewportCount = 1,
            .scissorCount = 1
        };

        // 5. Растеризатор (для PBR важен правильный обход граней)
        vk::PipelineRasterizationStateCreateInfo rasterizer {
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack, // Отсекаем задние грани для оптимизации
            .frontFace = vk::FrontFace::eCounterClockwise, // Направление обхода по часовой стрелке
            .depthBiasEnable = VK_FALSE, // Здесь смещение глубины выключено!
            .lineWidth = 1.0f
        };

        // 6. Мультисемплинг (выключен для простоты, 1 сэмпл)
        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE
        };

        // 7. Тест глубины (ОБЯЗАТЕЛЕН, иначе сцена превратится в кашу)
        vk::PipelineDepthStencilStateCreateInfo depthStencil {
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess, // Отрисовываем то, что ближе к камере
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE
        };

        // 8. Смешивание цветов (Color Blending)
        // Для непрозрачного PBR-материала смешивание выключено, но запись во все каналы включена
        vk::PipelineColorBlendAttachmentState colorBlendAttachment {
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending {
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };

        // 9. Динамические состояния (чтобы менять размер окна без пересоздания пайплайна)
        std::array dynamicStates {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState {
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        // 10. Финальная сборка Graphics Pipeline
        vk::GraphicsPipelineCreateInfo const pipelineCreateInfo {
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = mainPipelineLayout.get(), // Макет основного конвейера
            .renderPass = mainRenderPass.get()  // Проход основного рендеринга
        };

        auto mainPipelineResultValue = device.createGraphicsPipelineUnique(nullptr, pipelineCreateInfo);
        if (mainPipelineResultValue.result != vk::Result::eSuccess) {
            return mainPipelineResultValue.result;
        }
        mainPipeline = std::move(mainPipelineResultValue.value);

        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initShadowPipeline(vk::Device device) {
        // 1. Шейдеры (только Vertex, так как фрагментный для тени не нужен)
        auto vertModule = loadAndCreateShaderModuleUnique(device, "../shaders/shadow.vert.spv");
        if (vertModule.result != vk::Result::eSuccess) {
            return vertModule.result;
        }
        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertModule.value,
                .pName = "main"
            },
        };

        // Только для позиции
        vk::VertexInputBindingDescription vertexBindingDescription {
            .binding = 0,
            .stride = sizeof(PositionAttribute),
            .inputRate = vk::VertexInputRate::eVertex
        };

        // Атрибут для позиции
        vk::VertexInputAttributeDescription vertexAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = 0
        };

        // 2. Состояния
        vk::PipelineVertexInputStateCreateInfo vertexInput{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBindingDescription,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertexAttributeDescription
        }; // Твой формат вершин
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList };
        vk::PipelineViewportStateCreateInfo viewport{ .viewportCount = 1, .scissorCount = 1 };

        // ВАЖНО для теней: Depth Bias (против "теневых прыщей")
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_TRUE, // Включаем!
            .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess
        };

        // Нет ColorBlendState (так как нет цветового буфера!)
        vk::PipelineColorBlendStateCreateInfo colorBlend{ .logicOpEnable = VK_FALSE };

        // Динамические состояния для Viewport (чтобы использовать Атлас теней)
        vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eDepthBias };
        vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = 3, .pDynamicStates = dynamicStates };

        vk::GraphicsPipelineCreateInfo pipelineInfo{
            .stageCount = 1, .pStages = shaderStages,
            .pVertexInputState = &vertexInput, .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewport, .pRasterizationState = &rasterizer, .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil, .pColorBlendState = &colorBlend,
            .pDynamicState = &dynamicState,
            .layout = shadowPipelineLayout.get(),
            .renderPass = shadowRenderPass.get(),
            .subpass = 0
        };

        auto res = device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
        if (res.result != vk::Result::eSuccess) return res.result;
        shadowPipeline = std::move(res.value);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initSamplers(vk::Device device) {
        auto [createShadowSamplerResult, uniqueShadowSampler] = device.createSamplerUnique(
            vk::SamplerCreateInfo {
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eClampToBorder, // Граница карты теней
                .addressModeV = vk::SamplerAddressMode::eClampToBorder,
                .addressModeW = vk::SamplerAddressMode::eClampToBorder,
                // Manual PCF is done in pbr.frag via sampler2D(shadowMap, shadowSampler),
                // so hardware compare must stay disabled here.
                .compareEnable = vk::False,
                .compareOp = vk::CompareOp::eLess,
                .borderColor = vk::BorderColor::eFloatOpaqueWhite, // За границами - белый (свет)
            }
        );

        if (createShadowSamplerResult != vk::Result::eSuccess) {
            return createShadowSamplerResult;
        }

        auto [createMaterialSamplerResult, uniqueMaterialSampler] = device.createSamplerUnique(
            vk::SamplerCreateInfo {
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eRepeat, // Повторяем текстуру
                .addressModeV = vk::SamplerAddressMode::eRepeat,
                .addressModeW = vk::SamplerAddressMode::eRepeat,
                .anisotropyEnable = vk::True,                     // Включаем анизотропию
                .maxAnisotropy = 16.0f,                         // Максимальное качество
                .compareEnable = vk::False,                      // Сравнение не нужно
                .minLod = 0.0f,
                .maxLod = vk::LodClampNone                     // Используем все мип-уровни
            }
        );
        if (createMaterialSamplerResult != vk::Result::eSuccess) {
            return createMaterialSamplerResult;
        }

        shadowSampler = std::move(uniqueShadowSampler);
        materialSampler = std::move(uniqueMaterialSampler);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initSamplerDescriptorSet(vk::Device device) {
        vk::DescriptorPoolSize descriptorPoolSize{
            .type = vk::DescriptorType::eSampler,
            .descriptorCount = 2
        };

        auto [createSamplerDescriptorPoolResult, uniqueSamplerDescriptorPool] = device.createDescriptorPoolUnique(
            vk::DescriptorPoolCreateInfo {
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &descriptorPoolSize
            }
        );
        if (createSamplerDescriptorPoolResult != vk::Result::eSuccess) {
            return createSamplerDescriptorPoolResult;
        }

        samplerDescriptorPool = std::move(uniqueSamplerDescriptorPool);

        auto [createSamplerDescriptorSetResult, uniqueSamplerDescriptorSets] = device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo {
                .descriptorPool = *samplerDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &*samplersSetLayout
            }
        );

        if (createSamplerDescriptorSetResult != vk::Result::eSuccess) {
            return createSamplerDescriptorSetResult;
        }

        samplersSet = std::move(uniqueSamplerDescriptorSets[0]);

        std::array samplerDescriptorInfo {
            vk::DescriptorImageInfo {
                .sampler = *materialSampler
            },
            vk::DescriptorImageInfo {
                .sampler = *shadowSampler
            }
        };

        std::array samplerWriteDescriptorSet{
            vk::WriteDescriptorSet {
                .dstSet = samplersSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .pImageInfo = &samplerDescriptorInfo[0]
            },
            vk::WriteDescriptorSet {
                .dstSet = samplersSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .pImageInfo = &samplerDescriptorInfo[1]
            }
        };

        device.updateDescriptorSets(
            samplerWriteDescriptorSet,
            {}
        );
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initSamplerDescriptorSetLayout(vk::Device device) {
        std::array descriptorBindings {
            // MaterialSampler
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            // ShadowSampler
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            }
        };

        auto [createSamplerSetLayoutResult, uniqueSamplerSetLayout] = device.createDescriptorSetLayoutUnique(
            vk::DescriptorSetLayoutCreateInfo {
                .bindingCount = 2,
                .pBindings = descriptorBindings.data()
            }
        );
        if (createSamplerSetLayoutResult != vk::Result::eSuccess) {
            return createSamplerSetLayoutResult;
        }
        samplersSetLayout = std::move(uniqueSamplerSetLayout);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initModelDataSetLayout(vk::Device device) {
        vk::DescriptorSetLayoutBinding modelDataSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        };

        auto [createDescriptorSetLayoutResult, uniqueDescriptorSetLayout] = device.createDescriptorSetLayoutUnique(
            vk::DescriptorSetLayoutCreateInfo {
                .bindingCount = 1,
                .pBindings = &modelDataSetLayoutBinding
            }
        );
        if (createDescriptorSetLayoutResult != vk::Result::eSuccess) {
            return createDescriptorSetLayoutResult;
        }

        modelSetLayout = std::move(uniqueDescriptorSetLayout);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initPbrMaterialSetLayout(vk::Device device) {
        std::array bindings {
            // Binding 0: Material Properties
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            // Binding 1: Albedo Map
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            // Binding 2: Normal Map
            vk::DescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            // Binding 3: Ambient-Roughness-Metallic Map
            vk::DescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            // Binding 4: Emission Map
            vk::DescriptorSetLayoutBinding{
                .binding = 4,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            // Binding 5: Height Map
            vk::DescriptorSetLayoutBinding{
                .binding = 5,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            }
        };

        vk::DescriptorSetLayoutCreateInfo const createInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };

        auto res = device.createDescriptorSetLayoutUnique(createInfo);
        if (res.result != vk::Result::eSuccess) return res.result;

        pbrMaterialSetLayout = std::move(res.value);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initSceneDataSetLayout(vk::Device device) {
        std::array bindings {
            // Binding 0: Camera UBO (View/Proj, CameraPos)
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
            },

            // Binding 1: Light Info UBO (Метаданные: кол-во источников, Ambient)
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex
            },

            // Binding 2: Light SSBO (Массив DirectionalLightData: цвет, напрвление, матрицы теней)
            vk::DescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex
            },

            // Binding 3: Shadow Map Image (Только текстура/массив текстур)
            vk::DescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            }
        };

        vk::DescriptorSetLayoutCreateInfo const createInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };

        auto res = device.createDescriptorSetLayoutUnique(createInfo);
        if (res.result != vk::Result::eSuccess) return res.result;

        pbrSceneDataSetLayout = std::move(res.value);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initMainPipelineLayout(vk::Device device) {
        // ВАЖНО: порядок в массиве определяет номер set в шейдере: layout(set = 0, binding = X) и layout(set = 1, binding = Y)
        vk::DescriptorSetLayout const layouts[] = {
            samplersSetLayout.get(),     // Будет доступен как set = 0
            modelSetLayout.get(),        // Будет доступен как set = 1
            pbrSceneDataSetLayout.get(), // Будет доступен как set = 2
            pbrMaterialSetLayout.get(),  // Будет доступен как set = 3
        };

        vk::PipelineLayoutCreateInfo const createInfo{
            .setLayoutCount = std::size(layouts),
            .pSetLayouts = layouts
        };

        auto res = device.createPipelineLayoutUnique(createInfo);
        if (res.result != vk::Result::eSuccess) return res.result;

        mainPipelineLayout = std::move(res.value);
        return vk::Result::eSuccess;
    }

    vk::Result PbrRender::initShadowPipelineLayout(vk::Device device) {

        vk::DescriptorSetLayout const shadowLayouts[] {
            modelSetLayout.get(),
            pbrSceneDataSetLayout.get()
        };

        vk::PipelineLayoutCreateInfo const createInfo{
            .setLayoutCount = std::size(shadowLayouts),
            .pSetLayouts = shadowLayouts
        };

        auto res = device.createPipelineLayoutUnique(createInfo);
        if (res.result != vk::Result::eSuccess) return res.result;

        shadowPipelineLayout = std::move(res.value);
        return vk::Result::eSuccess;
    }
}