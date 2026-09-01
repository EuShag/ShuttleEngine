/**
 * @file Application.cpp
 * @brief Implementation of the main Shuttle Engine application.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Application.hpp"

#include "../../Assets/EnvironmentCompiler/include/Assets/EnvironmentCompiler/CpuIblGenerator.hpp"
#include "../../Assets/EnvironmentCompiler/src/CompiledEnvironmentBlobWriter.hpp"
#include "../Assets/SceneCompiler/src/Serialization/CompiledSceneBlobWriter.hpp"
#include "Assets/EnvironmentCompiler/EnvironmentCompiler.hpp"
#include "Assets/TextureCompiler/CompiledTexture.hpp"
#include "VulkanDebugger/VulkanDebugger.hpp"

namespace shuttle::engine::render
{

    namespace {
        struct InMemorySceneData
        {
            std::vector<assets::formats::texture::TextureMetadata> textureMetadatas;
            std::vector<assets::formats::texture::TextureMipMetadata> textureMipMetadatas;
            std::vector<uint8_t> textureBytes;
            LoadedSceneData loadedSceneData;
        };

        InMemorySceneData prepareInMemorySceneUpload(
            const assets::scene_compiler::CompiledScene& scene)
        {
            InMemorySceneData result{};
            uint64_t globalDataOffset = 0;
            uint64_t globalMipOffset = 0;

            result.textureMetadatas.reserve(scene.textures.size());

            for (const auto& texture : scene.textures)
            {
                auto metadata = texture.metadata;
                metadata.mipTableOffset = globalMipOffset;
                result.textureMetadatas.push_back(metadata);

                for (auto mip : texture.mipMetadata)
                {
                    mip.dataOffset += globalDataOffset;
                    result.textureMipMetadatas.push_back(mip);
                }

                globalMipOffset += texture.mipMetadata.size() *
                                   sizeof(assets::formats::texture::TextureMipMetadata);
                result.textureBytes.insert(
                    result.textureBytes.end(),
                    texture.data.begin(),
                    texture.data.end());
                globalDataOffset += texture.data.size();
            }

            result.loadedSceneData = LoadedSceneData{
                .nodes = scene.nodes,
                .levels = scene.levels,
                .transforms = scene.transforms,
                .drawables = scene.drawableObjects,
                .directionalLights = scene.directionalLights,
                .positions = scene.positions,
                .attributes = scene.attributes,
                .indices = scene.indices,
                .meshes = scene.meshes,
                .materials = scene.materials,
                .textureMetadatas = result.textureMetadatas,
                .textureMipMetadatas = result.textureMipMetadatas,
                .textureBytes = result.textureBytes
            };

            return result;
        }
    }

} // namespace shuttle::engine::render

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace shuttle::engine
{

    Application::Application(int argc, char** argv)
        : m_windowHandle(m_platform.createWindow(
            "Shuttle Engine", 1280, 720,
            pal::WindowType::Main,
            pal::WindowDecorationFlags::Default))
        , m_window(m_platform, m_windowHandle, "Shuttle Engine", 1280, 720)
        , m_camera(glm::vec3{10.0f, 30.3f, 0.0f})
        , m_cameraController(m_camera)
    {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        // Регистрируем Application как слушатель событий окна и ввода
        m_window.setWindowListener(this);
        m_window.setInputListener(this);

        // Получаем расширения поверхности от выбранной платформы (Win32 или SDL2)
        auto requiredSurfaceExtensions = pal::Platform::getSurfaceRequiredExtensions();
        requiredSurfaceExtensions.push_back(vk::EXTDebugUtilsExtensionName);

        VulkanDebugger debugger{};
        auto messengerCreateInfo = debugger.getDebugMessengerCreateInfo();

        vkb::InstanceBuilder instanceBuilder;
        auto instanceResult =
            instanceBuilder
                .set_app_name("Shuttle Engine - Adriatic Flight")
                .request_validation_layers(true)
                .set_engine_name("Shuttle Engine")
                .enable_extensions(requiredSurfaceExtensions)
                .set_debug_messenger_severity(messengerCreateInfo.messageSeverity)
                .set_debug_messenger_type(messengerCreateInfo.messageType)
                .set_debug_callback(messengerCreateInfo.pfnUserCallback)
                .require_api_version(VK_API_VERSION_1_4)
                .build();

        if (!instanceResult.has_value())
        {
            throw std::runtime_error(
                "Failed to create Vulkan instance: " +
                instanceResult.error().message());
        }

        std::cout << "[System] Vulkan instance created successfully.\n";

        vk::Instance instance{instanceResult.value().instance};
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

        m_uniqueInstance = vk::UniqueInstance{
            vk::Instance{instanceResult.value().instance},
            vk::UniqueHandleTraits<
                vk::Instance,
                VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                nullptr,
                VULKAN_HPP_DEFAULT_DISPATCHER}};

        m_messenger = vk::UniqueDebugUtilsMessengerEXT{
            vk::DebugUtilsMessengerEXT{instanceResult.value().debug_messenger},
            vk::UniqueHandleTraits<
                vk::DebugUtilsMessengerEXT,
                VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                instance,
                nullptr,
                VULKAN_HPP_DEFAULT_DISPATCHER}};

        // Создаем Vulkan Surface через абстракцию нашей Платформы
        auto [result, uniqueSurface] = createVulkanSurfaceUnique(*m_uniqueInstance, m_platform, m_windowHandle);

        if  (result != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Failed to create Vulkan surface: " +
                vk::to_string(result));
        }

        m_uniqueSurface = std::move(uniqueSurface);

        VkPhysicalDeviceFeatures2 features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features = vk::PhysicalDeviceFeatures{
                .imageCubeArray = vk::True,
                .multiDrawIndirect = vk::True,
                .drawIndirectFirstInstance = vk::True,
                .fillModeNonSolid = vk::True,
                .samplerAnisotropy = vk::True,
                .shaderInt64 = vk::True
            }};

        VkPhysicalDeviceVulkan11Features features11 =
            vk::PhysicalDeviceVulkan11Features{
                .multiview = vk::True,
                .shaderDrawParameters = vk::True};

        VkPhysicalDeviceVulkan12Features features12 =
            vk::PhysicalDeviceVulkan12Features{
                .drawIndirectCount = vk::True,
                .descriptorIndexing = vk::True,
                .shaderUniformBufferArrayNonUniformIndexing = vk::True,
                .shaderSampledImageArrayNonUniformIndexing = vk::True,
                .shaderStorageBufferArrayNonUniformIndexing = vk::True,
                .shaderStorageImageArrayNonUniformIndexing = vk::True,
                .shaderInputAttachmentArrayNonUniformIndexing = vk::True,
                .descriptorBindingUniformBufferUpdateAfterBind = vk::True,
                .descriptorBindingSampledImageUpdateAfterBind = vk::True,
                .descriptorBindingStorageImageUpdateAfterBind = vk::True,
                .descriptorBindingStorageBufferUpdateAfterBind = vk::True,
                .descriptorBindingUniformTexelBufferUpdateAfterBind = vk::True,
                .descriptorBindingStorageTexelBufferUpdateAfterBind = vk::True,
                .descriptorBindingPartiallyBound = vk::True,
                .descriptorBindingVariableDescriptorCount = vk::True,
                .runtimeDescriptorArray = vk::True,
                .scalarBlockLayout = vk::True,
                .uniformBufferStandardLayout = vk::True,
                .timelineSemaphore = vk::True,
                .bufferDeviceAddress = vk::True,
                .shaderOutputViewportIndex = vk::True,
                .shaderOutputLayer = vk::True};

        VkPhysicalDeviceVulkan13Features features13 =
            vk::PhysicalDeviceVulkan13Features{
                .inlineUniformBlock = vk::True,
                .descriptorBindingInlineUniformBlockUpdateAfterBind = vk::True,
                .pipelineCreationCacheControl = vk::True,
                .shaderDemoteToHelperInvocation = vk::True,
                .shaderTerminateInvocation = vk::True,
                .subgroupSizeControl = vk::True,
                .computeFullSubgroups = vk::True,
                .synchronization2 = vk::True,
                .shaderZeroInitializeWorkgroupMemory = vk::True,
                .dynamicRendering = vk::True,
                .shaderIntegerDotProduct = vk::True,
                .maintenance4 = vk::True};

        VkPhysicalDeviceVulkan14Features features14 =
            vk::PhysicalDeviceVulkan14Features{
                .shaderSubgroupRotate = vk::True,
                .shaderSubgroupRotateClustered = vk::True,
                .shaderExpectAssume = vk::True,
                .indexTypeUint8 = vk::True,
                .dynamicRenderingLocalRead = vk::True,
                .maintenance5 = vk::True,
                .maintenance6 = vk::True,
                .pipelineRobustness = vk::True,
                .pushDescriptor = vk::True};

        auto vkbPhysicalDeviceResult =
            vkb::PhysicalDeviceSelector{instanceResult.value(), *m_uniqueSurface}
                .set_minimum_version(1, 4)
                .add_required_extension(vk::KHRSwapchainExtensionName)
                .select();

        if (!vkbPhysicalDeviceResult.has_value())
        {
            throw std::runtime_error(
                "Failed to select physical device: " +
                vkbPhysicalDeviceResult.error().message());
        }

        std::cout << "[System] Physical device selected: "
                  << vkbPhysicalDeviceResult->properties.deviceName << '\n';

        auto vkbDeviceResult =
            vkb::DeviceBuilder{vkbPhysicalDeviceResult.value()}
                .set_allocation_callbacks(nullptr)
                .add_pNext(&features2)
                .add_pNext(&features11)
                .add_pNext(&features12)
                .add_pNext(&features13)
                .add_pNext(&features14)
                .build();

        if (!vkbDeviceResult.has_value())
        {
            throw std::runtime_error(
                "Failed to create logical device: " +
                vkbDeviceResult.error().message());
        }

        std::cout << "[System] Vulkan logical device created successfully.\n";

        auto graphicsQueueResult =
            vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::graphics);
        auto computeQueueResult =
            vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::compute);
        auto transferQueueResult =
            vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::transfer);
        auto presentationQueueResult =
            vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::present);

        if (!graphicsQueueResult.has_value() ||
            !presentationQueueResult.has_value() ||
            !computeQueueResult.has_value() ||
            !transferQueueResult.has_value())
        {
            throw std::runtime_error("Failed to retrieve required queues from the logical device");
        }

        m_graphicsQueue = graphicsQueueResult.value().first;
        m_graphicsQueueFamilyIndex = graphicsQueueResult.value().second;
        m_physicalDevice = vkbPhysicalDeviceResult.value().physical_device;
        m_device = vkbDeviceResult.value().device;

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
        m_uniqueDevice = vk::UniqueDevice{
            m_device,
            vk::UniqueHandleTraits<
                vk::Device,
                VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                nullptr,
                VULKAN_HPP_DEFAULT_DISPATCHER}};

        auto [createAllocatorResult, uniqueAllocator] =
            resources::UniqueAllocator::makeUnique(
                *m_uniqueInstance,
                *m_uniqueDevice,
                m_physicalDevice,
                vk::detail::defaultDispatchLoaderDynamic);

        if (createAllocatorResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create VMA allocator");
        }

        m_allocator = std::move(uniqueAllocator);

        m_swapchainContext = SwapchainContext{
            .physicalDevice = m_physicalDevice,
            .device = m_device,
            .surface = *m_uniqueSurface,
            .graphicsQueueFamily = m_graphicsQueueFamilyIndex,
            .presentQueueFamily = m_graphicsQueueFamilyIndex};

        vk::Extent2D windowExtent{ m_window.getWidth(), m_window.getHeight() };
        auto [createSwapchainResult, swapchain] =
            createSwapchain(m_swapchainContext, windowExtent);

        if (createSwapchainResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create swapchain");
        }

        auto [createFrameManagerResult, frameManager] =
            FrameManager::create(m_device, m_frameCount, swapchain.imageCount);

        if (createFrameManagerResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create frameManager");
        }

        std::array descriptorPoolSizes{
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampler,
                .descriptorCount = 16},
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 64},
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 512},
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampledImage,
                .descriptorCount = render::MaxBindlessTextures + 128},
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageImage,
                .descriptorCount = 128}};

        auto [createRenderDescriptorPoolResult, renderDescriptorPool] =
            m_uniqueDevice->createDescriptorPoolUnique({
                .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
                .maxSets = 64,
                .poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
                .pPoolSizes = descriptorPoolSizes.data()});

        if (createRenderDescriptorPoolResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create render descriptor pool");
        }

        m_renderContext = render::RenderContext{
            .device = m_device,
            .allocator = *m_allocator,
            .swapchainColorFormat = swapchain.format,
            .swapchainDepthFormat = vk::Format::eD32Sfloat,
            .descriptorPool = std::move(renderDescriptorPool)};

        auto [createUniqueGraphicsCommandPoolResult, uniqueGraphicsCommandPool_] =
            m_uniqueDevice->createCommandPoolUnique({
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = m_graphicsQueueFamilyIndex});

        if (createUniqueGraphicsCommandPoolResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create graphics command pool");
        }

        m_uniqueGraphicsCommandPool = std::move(uniqueGraphicsCommandPool_);

        auto [createDescriptorHeapSetResult, m_descriptorHeapSet_] =
            render::DescriptorHeapSet::create(
                m_device,
                render::DescriptorHeapSetCreateInfo{
                    .textureCount = render::MaxBindlessTextures,
                    .samplerCount = 16,
                    .storageImageCount = 128,
                    .uniformTexelBufferCount = 64,
                    .storageTexelBufferCount = 64,
                    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics |
                                  vk::ShaderStageFlagBits::eCompute});

        if (createDescriptorHeapSetResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create descriptor heap set");
        }

        m_descriptorHeapSet = std::move(m_descriptorHeapSet_);

        auto [createCommonResourcesResult, commonResources_] =
            render::createCommonResources(
                m_device,
                m_graphicsQueue,
                *m_uniqueGraphicsCommandPool,
                *m_allocator,
                m_descriptorHeapSet);

        if (createCommonResourcesResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create common resources");
        }

        m_commonResources = std::move(commonResources_);

        auto [createFallbackTexturesResult, fallbackTextures_] =
            render::createFallbackTextures(
                m_device,
                m_graphicsQueue,
                *m_uniqueGraphicsCommandPool,
                *m_allocator,
                m_descriptorHeapSet);

        if (createFallbackTexturesResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create fallback textures");
        }

        m_fallbackTextures = std::move(fallbackTextures_);

        m_camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        m_camera.setWindowSize(swapchain.extent.width, swapchain.extent.height);

        // Создаем UiRender через универсальный WindowBase (m_window)
        auto [uiRenderResult, uiRenderObj] =
            UiRender::create(
                m_window,
                instance,
                m_physicalDevice,
                m_device,
                m_graphicsQueueFamilyIndex,
                m_graphicsQueue,
                swapchain.imageCount);

        if (uiRenderResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create UI render");
        }

        render::MainPassSettings mainPassSettings{};
        m_uiRender = std::move(uiRenderObj);

        m_mainWindow = editor::core::MainWindow(&m_window, mainPassSettings, false);

        auto [allocateGraphicsCommandBufferResult, uniqueGraphicsCommandBuffers_] =
            m_device.allocateCommandBuffersUnique({
                .commandPool = *m_uniqueGraphicsCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = m_frameCount});

        if (allocateGraphicsCommandBufferResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create graphics command buffer");
        }

        m_uniqueGraphicsCommandBuffers = std::move(uniqueGraphicsCommandBuffers_);

        m_activeResources = SwapchainResources{
            .swapchain = std::move(swapchain),
            .frameManager = std::move(frameManager)};
        m_swapchainImageLayouts.resize(
            m_activeResources.swapchain.imageCount,
            vk::ImageLayout::eUndefined);

        m_depthAttachmentOutputs.resize(m_frameCount);
        m_colorAttachmentOutputs.resize(m_frameCount);
        m_debugOutputs1.resize(m_frameCount);
        m_debugOutputs2.resize(m_frameCount);
        m_debugOutputs3.resize(m_frameCount);
        m_debugOutputs4.resize(m_frameCount);

        m_colorAttachmentSets.resize(m_frameCount);
        m_debugAttachmentSets1.resize(m_frameCount);
        m_debugAttachmentSets2.resize(m_frameCount);
        m_debugAttachmentSets3.resize(m_frameCount);
        m_debugAttachmentSets4.resize(m_frameCount);

        auto descriptorSetLayout = m_descriptorHeapSet.getDescriptorSetLayout();

        vk::PushConstantRange pushConstantRange{
            .stageFlags =
                vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = 128};

        auto [createPipelineLayoutResult, pipelineLayout_] =
            m_uniqueDevice->createPipelineLayoutUnique({
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange});

        if (createPipelineLayoutResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        m_pipelineLayout = std::move(pipelineLayout_);

        auto [createWorldTransformUpdatePassResult, worldTransformUpdatePass_] =
            render::WorldTransformUpdatePass::create(m_device, *m_pipelineLayout);

        if (createWorldTransformUpdatePassResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create world transform update pass");
        }

        m_worldTransformUpdatePass = std::move(worldTransformUpdatePass_);

        auto [createMeshInstanceCountPassResult, meshInstanceCountPass_] =
            render::MeshInstancesCountPass::create(m_device, *m_pipelineLayout);

        if (createMeshInstanceCountPassResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create mesh instance count pass");
        }

        m_meshInstanceCountPass = std::move(meshInstanceCountPass_);

        auto [createPrefixSumPassResult, prefixSumPass_] =
            render::PrefixSumPass::create(m_device, *m_pipelineLayout);

        if (createPrefixSumPassResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create prefix sum pass");
        }

        m_prefixSumPass = std::move(prefixSumPass_);

        auto [createInstanceRemapPassResult, instanceRemapPass_] =
            render::InstanceRemapPass::create(m_device, *m_pipelineLayout);

        if (createInstanceRemapPassResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create instance remap pass");
        }

        m_instanceRemapPass = std::move(instanceRemapPass_);

        auto [createMainRenderPassResult, mainRenderPass_] =
            render::MainRenderPass::create(
                m_device,
                *m_pipelineLayout,
                m_renderContext.swapchainColorFormat,
                m_renderContext.swapchainDepthFormat);

        if (createMainRenderPassResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create main render pass");
        }

        m_mainRenderPass = std::move(mainRenderPass_);

        m_cameraSystems.resize(m_frameCount);
        m_mainPassSettingSystems.resize(m_frameCount);
        m_worldTransformBuffers.resize(m_frameCount);
        m_instanceRemapBuffers.resize(m_frameCount);
        m_meshInstanceCursorBuffers.resize(m_frameCount);
        m_indirectDrawCommandsBuffers.resize(m_frameCount);
        m_worldTransformBufferAddresses.resize(m_frameCount);
        m_instanceRemapBufferAddresses.resize(m_frameCount);
        m_meshInstanceCursorBufferAddresses.resize(m_frameCount);
        m_indirectDrawCommandsBufferAddresses.resize(m_frameCount);

        m_mainWindow.setOpenSceneCallback(
            [this](std::filesystem::path const& path)
            {
                loadScene(path);
            });

        m_mainWindow.setOpenEnvironmentCallback(
            [this](std::filesystem::path const& path)
            {
                loadEnvironment(path);
            });

        m_mainWindow.setImportSceneCallback(
            [this](
                std::filesystem::path const& inputPath,
                assets::scene_compiler::SceneCompilerOptions const& options)
            {
                m_hasFrameResources = false;
                if (auto waitForResult = m_activeResources.frameManager.waitRenderIdle(m_device);
                    waitForResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error(
                        "Failed to wait for render idle: " +
                        std::to_string(static_cast<int>(waitForResult)));
                }

                auto compiledSceneOpt =
                    shuttle::assets::scene_compiler::SceneCompiler::compile(inputPath, options);

                if (!compiledSceneOpt)
                {
                    std::cerr << "[Editor] Failed to compile imported asset into memory.\n";
                    return;
                }

                auto inMemoryData =
                    render::prepareInMemorySceneUpload(*compiledSceneOpt);

                auto [uploadResult, uploadOutput] = render::uploadScene(
                    inMemoryData.loadedSceneData,
                    m_renderContext,
                    m_graphicsQueue,
                    *m_uniqueGraphicsCommandPool,
                    m_descriptorHeapSet,
                    m_fallbackTextures.indices);

                if (uploadResult != vk::Result::eSuccess)
                {
                    return;
                }

                OpenAsset newAsset{
                    .id = m_nextId++,
                    .name = inputPath.stem().string() + " [Unsaved]",
                    .path = inputPath,
                    .isScene = true,
                    .isDirty = true,
                    .sceneGpuData = std::move(uploadOutput),
                    .compiledSceneRAM = std::move(*compiledSceneOpt)};

                m_mainWindow.addAsset(editor::core::LoadedAsset{
                    .name = newAsset.name,
                    .path = newAsset.path,
                    .isDirty = true,
                    .id = newAsset.id,
                    .type = editor::core::AssetType::Scene,
                    .isScene = true});

                m_openAssets.push_back(std::move(newAsset));
                m_activeSceneId = m_openAssets.back().id;

            });

        m_mainWindow.setImportEnvironmentCallback(
            [this](
                std::filesystem::path const& inputPath,
                ibl::IblGenerationSettings const& settings)
            {
                m_hasFrameResources = false;
                if (auto waitForResult = m_activeResources.frameManager.waitRenderIdle(m_device);
                    waitForResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error(
                        "Failed to wait for render idle: " +
                        std::to_string(static_cast<int>(waitForResult)));
                }

                auto compiledEnvOpt =
                    assets::environment_compiler::EnvironmentCompiler::compile(
                        inputPath,
                        settings);

                if (!compiledEnvOpt)
                {
                    std::cerr << "[Editor] Failed to compile imported environment into memory.\n";
                    return;
                }

                std::filesystem::path outputPath = inputPath;
                outputPath.replace_extension(".env");

                if (!assets::environment_compiler::CompiledEnvironmentBlobWriter::write(
                        *compiledEnvOpt,
                        outputPath))
                {
                    throw std::runtime_error(
                        "Failed to write compiled environment to disk: " +
                        outputPath.string());
                }

                auto [createEnvironmentResourcesResult, environmentResources_] =
                    render::createEnvironmentResources(
                        m_renderContext,
                        m_graphicsQueue,
                        *m_uniqueGraphicsCommandPool,
                        m_descriptorHeapSet,
                        outputPath);

                OpenAsset newAsset{
                    .id = m_nextId++,
                    .name = inputPath.stem().string() + " [Unsaved]",
                    .path = inputPath,
                    .isScene = false,
                    .isDirty = true,
                    .envGpuData = std::move(environmentResources_),
                    .compiledEnvRAM = std::move(*compiledEnvOpt)
                };

                m_mainWindow.addAsset(editor::core::LoadedAsset{
                    .name = newAsset.name,
                    .path = newAsset.path,
                    .isDirty = true,
                    .id = newAsset.id,
                    .type = shuttle::editor::core::AssetType::Environment,
                    .isScene = false});

                m_openAssets.push_back(std::move(newAsset));
                m_activeEnvironmentId = m_openAssets.back().id;

            });

        m_mainWindow.setSaveSceneCallback(
            [this](std::filesystem::path const& savePath)
            {
                if (m_activeSceneId != 0)
                {
                    auto it = std::ranges::find_if(
                        m_openAssets, [&](const OpenAsset& a)
                        {
                            return a.id == m_activeSceneId && a.isScene;
                        });

                    if (it != m_openAssets.end() && it->compiledSceneRAM.has_value())
                    {
                        bool success =
                            shuttle::assets::scene_compiler::CompiledSceneBlobWriter::write(
                                *it->compiledSceneRAM,
                                savePath);

                        if (success)
                        {
                            it->isDirty = false;
                            it->name = savePath.filename().string();
                            it->path = savePath;
                            m_mainWindow.markAssetSaved(it->id, it->name, it->path);
                            std::cout << "[Editor] Asset successfully saved to disk: "
                                      << savePath << '\n';
                        }
                    }
                }
            });

        m_mainWindow.setSaveEnvironmentCallback(
            [this](std::filesystem::path const& savePath)
            {
                if (m_activeEnvironmentId != 0)
                {
                    auto it = std::ranges::find_if(
                        m_openAssets, [&](const OpenAsset& a) {
                            return a.id == m_activeEnvironmentId && !a.isScene;
                        });

                    if (it != m_openAssets.end() && it->compiledEnvRAM.has_value())
                    {
                        bool success =
                            shuttle::assets::environment_compiler::CompiledEnvironmentBlobWriter::write(
                                *it->compiledEnvRAM,
                                savePath);

                        if (success)
                        {
                            it->isDirty = false;
                            it->name = savePath.filename().string();
                            it->path = savePath;
                            m_mainWindow.markAssetSaved(it->id, it->name, it->path);
                            std::cout << "[Editor] Environment asset successfully saved to disk: "
                                      << savePath << '\n';
                        }
                        else
                        {
                            std::cerr << "[Editor] Failed to save environment asset to disk: "
                                      << savePath << '\n';
                        }
                    }
                }
            });

        m_mainWindow.setSelectAssetCallback(
            [this](editor::core::ResourceId id)
            {
                auto it = std::ranges::find_if(
                    m_openAssets, [id](const OpenAsset& a) {
                        return a.id == id;
                    });

                if (it != m_openAssets.end())
                {
                    if (it->isScene)
                    {
                        m_activeSceneId = id;
                        m_hasFrameResources = false;
                        if (auto waitForResult = m_activeResources.frameManager.waitRenderIdle(m_device);
                            waitForResult != vk::Result::eSuccess)
                        {
                            throw std::runtime_error(
                                "Failed to wait for render idle: " +
                                std::to_string(static_cast<int>(waitForResult)));
                        }
                    }
                    else
                    {
                        m_activeEnvironmentId = id;
                    }
                }
            });

        m_mainWindow.setCloseAssetCallback(
            [this](editor::core::ResourceId id)
            {
                if (auto result = m_activeResources.frameManager.waitRenderIdle(m_device);
                    result != vk::Result::eSuccess)
                {
                    std::cerr << "Failed to wait for render idle: "
                              << static_cast<int>(result)
                              << '\n';
                }

                auto it = std::ranges::find_if(
                    m_openAssets, [id](const OpenAsset& asset) {
                        return asset.id == id;
                    });

                if (it == m_openAssets.end())
                {
                    return;
                }

                if (it->isScene)
                {
                    if (m_activeSceneId == id)
                    {
                        m_activeSceneId = 0;
                        m_hasFrameResources = false;
                    }
                }
                else
                {
                    if (m_activeEnvironmentId == id)
                    {
                        m_activeEnvironmentId = 0;
                    }
                }

                std::erase_if(
                    m_openAssets,
                    [id](const OpenAsset& asset)
                    {
                        return asset.id == id;
                    });

                m_hasFrameResources = false;
            });

        m_lastTime = std::chrono::high_resolution_clock::now();

        // Показываем окно после завершения всей инициализации
        m_window.show();
    }

    Application::~Application()
    {
        if (m_uniqueDevice)
        {
            if (m_uniqueDevice->waitIdle() != vk::Result::eSuccess)
            {
                std::cerr << "Failed to wait for device idle during shutdown\n";
            }
        }

        std::cout << "[Shutdown] Device idle confirmed. Exiting cleanly.\n";
    }

    // ---------------------------------------------------------------------
    // ОБРАБОТЧИКИ СОБЫТИЙ СЛУШАТЕЛЕЙ (PAL Event Listeners)
    // ---------------------------------------------------------------------

    void Application::onWindowResize(const pal::WindowResizeEvent& event)
    {
        // При изменении размеров нативного окна
    }

    void Application::onWindowCloseRequested()
    {
        std::cout << "[System] Window close event received, posting quit...\n";
        m_platform.postQuitEvent();
    }

    void Application::onWindowPaint() {
        drawFrame(true, m_deltaTime);
    }

    void Application::onKeyboard(const input::KeyboardEvent& event)
    {
        m_cameraController.onKeyboard(event);
        if (event.state == pal::KeyState::Pressed && event.key == pal::KeyCode::Escape)
        {
            std::cout << "[System] Escape key pressed, closing application...\n";
            m_platform.postQuitEvent();
        }
    }

    void Application::createViewPortResources(vk::Extent2D viewportExtent)
    {
        m_camera.setWindowSize(viewportExtent.width, viewportExtent.height);

        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            auto [createColorAttachmentOutputResult, colorAttachmentOutput] =
                createAttachmentOutput(
                    m_device,
                    *m_allocator,
                    {
                        .format = m_renderContext.swapchainColorFormat,
                        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                      vk::ImageUsageFlagBits::eSampled,
                        .imageSize = viewportExtent,
                        .imageAspectFlags = vk::ImageAspectFlagBits::eColor,
                    });

            if (createColorAttachmentOutputResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Failed to create color attachment output");
            }

            auto [createDepthAttachmentOutputResult, depthAttachmentOutput] =
                createAttachmentOutput(
                    m_device,
                    *m_allocator,
                    {
                        .format = m_renderContext.swapchainDepthFormat,
                        .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                        .imageSize = viewportExtent,
                        .imageAspectFlags = vk::ImageAspectFlagBits::eDepth
                    });

            if (createDepthAttachmentOutputResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Failed to create depth attachment output");
            }

            auto createDebugOutputResources = [&]() -> AttachmentOutput
            {
                auto [createDebugOutputResult, debugOutputs] =
                    createAttachmentOutput(
                        m_device,
                        *m_allocator,
                        {
                            .format = vk::Format::eR16G16B16A16Sfloat,
                            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                          vk::ImageUsageFlagBits::eSampled,
                            .imageSize = viewportExtent,
                            .imageAspectFlags = vk::ImageAspectFlagBits::eColor,
                        });

                if (createDebugOutputResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Failed to create debug attachment output");
                }
                return std::move(debugOutputs);
            };

            m_debugOutputs1[i] = std::move(createDebugOutputResources());
            m_debugOutputs2[i] = std::move(createDebugOutputResources());
            m_debugOutputs3[i] = std::move(createDebugOutputResources());
            m_debugOutputs4[i] = std::move(createDebugOutputResources());

            m_colorAttachmentOutputs[i] = std::move(colorAttachmentOutput);
            m_depthAttachmentOutputs[i] = std::move(depthAttachmentOutput);

            m_colorAttachmentSets[i] =
                editor::core::MainWindow::createViewportDescriptorSetUnique(
                    *m_colorAttachmentOutputs[i].view);
            m_debugAttachmentSets1[i] =
                editor::core::MainWindow::createViewportDescriptorSetUnique(
                    *m_debugOutputs1[i].view);
            m_debugAttachmentSets2[i] =
                editor::core::MainWindow::createViewportDescriptorSetUnique(
                    *m_debugOutputs2[i].view);
            m_debugAttachmentSets3[i] =
                editor::core::MainWindow::createViewportDescriptorSetUnique(
                    *m_debugOutputs3[i].view);
            m_debugAttachmentSets4[i] =
                editor::core::MainWindow::createViewportDescriptorSetUnique(
                    *m_debugOutputs4[i].view);
        }
    }

    void Application::recreateViewportResources(vk::Extent2D viewportExtent)
    {
        m_camera.setWindowSize(viewportExtent.width, viewportExtent.height);

        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            m_resourceBin.retireImage(
                std::move(m_colorAttachmentOutputs[i].image),
                std::move(m_colorAttachmentOutputs[i].view),
                i);
            m_resourceBin.retireImage(
                std::move(m_depthAttachmentOutputs[i].image),
                std::move(m_depthAttachmentOutputs[i].view),
                i);
            m_resourceBin.retireImage(
                std::move(m_debugOutputs1[i].image),
                std::move(m_debugOutputs1[i].view),
                i);
            m_resourceBin.retireImage(
                std::move(m_debugOutputs2[i].image),
                std::move(m_debugOutputs2[i].view),
                i);
            m_resourceBin.retireImage(
                std::move(m_debugOutputs3[i].image),
                std::move(m_debugOutputs3[i].view),
                i);
            m_resourceBin.retireImage(
                std::move(m_debugOutputs4[i].image),
                std::move(m_debugOutputs4[i].view),
                i);
            m_resourceBin.retireDescriptorSet(
                std::move(m_colorAttachmentSets[i]),
                i);
            m_resourceBin.retireDescriptorSet(
                std::move(m_debugAttachmentSets1[i]),
                i);
            m_resourceBin.retireDescriptorSet(
                std::move(m_debugAttachmentSets2[i]),
                i);
            m_resourceBin.retireDescriptorSet(
                std::move(m_debugAttachmentSets3[i]),
                i);
            m_resourceBin.retireDescriptorSet(
                std::move(m_debugAttachmentSets4[i]),
                i);
        }

        createViewPortResources(viewportExtent);
    }

    void Application::recreateAllResources()
    {
        vk::Extent2D windowExtent{ m_window.getWidth(), m_window.getHeight() };
        auto [result, newResources] = m_retireController.updateSwapchainResources(
            m_swapchainContext,
            windowExtent,
            m_frameCount,
            std::move(m_activeResources));

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Fatal: Swapchain recreation failed");
        }

        m_activeResources = std::move(newResources);
        m_camera.setWindowSize(
            m_activeResources.swapchain.extent.width,
            m_activeResources.swapchain.extent.height);
    }

    const render::UploadSceneOutput* Application::findActiveScene() const
    {
        if (m_activeSceneId != 0)
        {
            auto it = std::ranges::find_if(
                m_openAssets, [&](const OpenAsset& a) {
                    return a.id == m_activeSceneId && a.isScene;
                });

            if (it != m_openAssets.end() && it->sceneGpuData.has_value())
            {
                return &it->sceneGpuData.value();
            }
        }

        for (const auto& asset : m_openAssets)
        {
            if (asset.isScene && asset.sceneGpuData.has_value())
            {
                return &asset.sceneGpuData.value();
            }
        }

        return nullptr;
    }

    const render::DeviceEnvironmentResources* Application::findActiveEnvironment() const
    {
        if (m_activeEnvironmentId != 0)
        {
            auto const it = std::ranges::find_if(
                m_openAssets, [&](const OpenAsset& a) {
                    return a.id == m_activeEnvironmentId && !a.isScene;
                });

            if (it != m_openAssets.end() && it->envGpuData.has_value())
            {
                return &it->envGpuData.value();
            }
        }

        for (const auto& asset : m_openAssets)
        {
            if (!asset.isScene && asset.envGpuData.has_value())
            {
                return &asset.envGpuData.value();
            }
        }

        return nullptr;
    }

    void Application::loadScene(std::filesystem::path const& path)
    {
        m_hasFrameResources = false;
        if (auto waitForResult = m_activeResources.frameManager.waitRenderIdle(m_device);
            waitForResult != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Failed to wait for render idle: " +
                std::to_string(static_cast<int>(waitForResult)));
        }

        auto loadedSceneData = render::loadSceneData(path);
        if (loadedSceneData.result != vk::Result::eSuccess)
        {
            return;
        }

        auto [uploadSceneResult, uploadSceneOutput] = render::uploadScene(
            *loadedSceneData,
            m_renderContext,
            m_graphicsQueue,
            *m_uniqueGraphicsCommandPool,
            m_descriptorHeapSet,
            m_fallbackTextures.indices);

        if (uploadSceneResult != vk::Result::eSuccess)
        {
            return;
        }

        OpenAsset item{
            .id = m_nextId++,
            .name = path.filename().string(),
            .path = path,
            .isScene = true,
            .isDirty = false,
            .sceneGpuData = std::move(uploadSceneOutput)};

        m_mainWindow.addAsset(editor::core::LoadedAsset{
            .name = item.name,
            .path = item.path,
            .isDirty = false,
            .id = item.id,
            .type = shuttle::editor::core::AssetType::Scene,
            .isScene = true});

        m_openAssets.push_back(std::move(item));
        m_activeSceneId = m_openAssets.back().id;

    }

    void Application::loadEnvironment(std::filesystem::path const& path)
    {
        m_hasFrameResources = false;
        if (auto waitForResult = m_activeResources.frameManager.waitRenderIdle(m_device);
            waitForResult != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Failed to wait for render idle: " +
                std::to_string(static_cast<int>(waitForResult)));
        }

        auto [createEnvironmentResourcesResult, environmentResources_] =
            render::createEnvironmentResources(
                m_renderContext,
                m_graphicsQueue,
                *m_uniqueGraphicsCommandPool,
                m_descriptorHeapSet,
                path);

        if (createEnvironmentResourcesResult != vk::Result::eSuccess)
        {
            return;
        }

        OpenAsset item{
            .id = m_nextId++,
            .name = path.filename().string(),
            .path = path,
            .isScene = false,
            .isDirty = false,
            .envGpuData = std::move(environmentResources_)};

        m_mainWindow.addAsset(editor::core::LoadedAsset{
            .name = item.name,
            .path = item.path,
            .isDirty = false,
            .id = item.id,
            .type = editor::core::AssetType::Environment,
            .isScene = false});

        m_openAssets.push_back(std::move(item));
        m_activeEnvironmentId = m_openAssets.back().id;
    }

    void Application::updateFrameData()
    {
        auto const* scene = findActiveScene();
        if (scene == nullptr)
        {
            throw std::runtime_error("Fatal: Scene resources are not available");
        }

        for (int i = 0; i < m_frameCount; i++)
        {
            auto [createWorldTransformBufferResult, worldTransformBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(glm::mat4) * scene->hostSceneData.nodes.size(),
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createWorldTransformBufferResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Fatal: Failed to create world transform buffer");
            }
            m_worldTransformBuffers[i] = std::move(worldTransformBuffer);

            auto [createInstanceRemapBufferResult, instanceRemapBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(uint32_t) *
                                scene->hostSceneData.drawableObjects.size(),
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createInstanceRemapBufferResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Fatal: Failed to create instance remap buffer");
            }
            m_instanceRemapBuffers[i] = std::move(instanceRemapBuffer);

            auto [createMeshInstanceCursorBufferResult, meshInstanceCursorBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(uint32_t) *
                                scene->sceneFrameRequirements.meshCount,
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createMeshInstanceCursorBufferResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Fatal: Failed to create mesh instance cursor buffer");
            }
            m_meshInstanceCursorBuffers[i] = std::move(meshInstanceCursorBuffer);

            auto [createIndirectDrawCommandsBufferResult, indirectDrawCommandsBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(vk::DrawIndexedIndirectCommand) *
                                scene->sceneFrameRequirements.meshCount,
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eIndirectBuffer,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createIndirectDrawCommandsBufferResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Fatal: Failed to create indirect draw commands buffer");
            }
            m_indirectDrawCommandsBuffers[i] = std::move(indirectDrawCommandsBuffer);

            m_worldTransformBufferAddresses[i] = m_device.getBufferAddress({.buffer = *m_worldTransformBuffers[i]});
            m_instanceRemapBufferAddresses[i] = m_device.getBufferAddress({.buffer = *m_instanceRemapBuffers[i]});
            m_meshInstanceCursorBufferAddresses[i] = m_device.getBufferAddress({.buffer = *m_meshInstanceCursorBuffers[i]});
            m_indirectDrawCommandsBufferAddresses[i] = m_device.getBufferAddress({.buffer = *m_indirectDrawCommandsBuffers[i]});

            auto [createCameraSystemResult, m_cameraSystem] =
                render::CameraSystem::create(m_device, *m_allocator);

            if (createCameraSystemResult != vk::Result::eSuccess)
            {
                throw std::runtime_error(
                    "Fatal: Failed to create camera system: " +
                    vk::to_string(createCameraSystemResult));
            }
            m_cameraSystems[i] = std::move(m_cameraSystem);

            auto [createMainPassSettingSystemResult, mainPassSettingSystem] =
                render::MainPassSettingSystem::create(m_device, *m_allocator);

            if (createMainPassSettingSystemResult != vk::Result::eSuccess)
            {
                throw std::runtime_error(
                    "Fatal: Failed to create main pass setting system: " +
                    vk::to_string(createMainPassSettingSystemResult));
            }
            m_mainPassSettingSystems[i] = std::move(mainPassSettingSystem);
        }

        m_hasFrameResources = true;
    }

    vk::ResultValue<Application::RenderIndices> Application::prepareFrame() {
        auto [prepareRes, currentFrameIndex] = m_activeResources.frameManager.acquireFrameSlot(m_device);

        if (prepareRes != vk::Result::eSuccess) {
            return {prepareRes, {}};
        }
        m_retireController.renderRetireUpdate(currentFrameIndex);
        m_resourceBin.release(currentFrameIndex);
        auto [acquireResult, currentImageIndex] = m_activeResources.frameManager.acquireNextImage(
            m_device,
            *m_activeResources.swapchain.swapchain,
            currentFrameIndex);
        if (acquireResult != vk::Result::eSuccess) {
            return {acquireResult, {}};
        }
        m_retireController.presentRetireUpdate(currentImageIndex);
        return {vk::Result::eSuccess, {.frameIndex = currentFrameIndex, .imageIndex = currentImageIndex}};
    }

    void Application::doFrameRender(
        RenderIndices renderIndices,
        render::UploadSceneOutput const& scene,
        render::DeviceEnvironmentResources const& environment,
        bool isResizeMode) {

        if (m_hasFrameResources)
        {
            m_mainWindow.setFinalViewportImage(
                *m_colorAttachmentSets[renderIndices.frameIndex]);
            m_mainWindow.setDebugViewportImages({
                *m_debugAttachmentSets1[renderIndices.frameIndex],
                *m_debugAttachmentSets2[renderIndices.frameIndex],
                *m_debugAttachmentSets3[renderIndices.frameIndex],
                *m_debugAttachmentSets4[renderIndices.frameIndex]});
        }

        render::UiPass::drawUi(m_mainWindow, m_platform);

        if (m_hasFrameResources && !isResizeMode)
        {
            if (!m_mainWindow.hasViewport())
            {
                createViewPortResources(m_mainWindow.getViewportExtent());
            }
            if (m_mainWindow.needViewPortResourcesRecreate())
            {
                recreateViewportResources(m_mainWindow.getViewportExtent());
            }
        }

        m_retireController.presentRetireUpdate(renderIndices.imageIndex);
        if (auto result = m_activeResources.frameManager.beginFrame(m_device, renderIndices.frameIndex);
            result != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Fatal: Failed to begin frame: " + vk::to_string(result));
        }

        uint32_t const imageIndex = renderIndices.imageIndex;

        vk::CommandBuffer cmd = *m_uniqueGraphicsCommandBuffers[renderIndices.frameIndex];
        if (auto resetResult = cmd.reset(); resetResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to reset graphics command buffer");
        }

        if (auto beginResult = cmd.begin(
                {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
            beginResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to begin graphics command buffer");
        }

        if (m_hasFrameResources && !isResizeMode) {
            m_cameraSystems[renderIndices.frameIndex].updateData(m_camera);

            render::RenderRootData renderRootData{
                .commonDataDeviceAddress = m_commonResources.infoAddress,
                .sceneDataDeviceAddress = m_device.getBufferAddress({.buffer = *scene.deviceSceneResources.sceneRootBuffer}),
                .environmentDataDeviceAddress = m_device.getBufferAddress({.buffer = *environment.environmentBuffer}),
                .cameraDataDeviceAddress = m_cameraSystems[renderIndices.frameIndex].getCameraDataAddress()};

            cmd.pushConstants(
                *m_pipelineLayout,
                vk::ShaderStageFlagBits::eVertex |
                    vk::ShaderStageFlagBits::eFragment |
                    vk::ShaderStageFlagBits::eCompute,
                0,
                sizeof(renderRootData),
                &renderRootData);

            auto heapDescriptorSet = m_descriptorHeapSet.getDescriptorSet();

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *m_pipelineLayout,
                0, 1,
                &heapDescriptorSet,
                0, nullptr);

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                *m_pipelineLayout,
                0, 1,
                &heapDescriptorSet,
                0, nullptr);

            cmd.bindIndexBuffer(
                *scene.deviceSceneResources.indexBuffer,
                0,
                vk::IndexType::eUint32);

            render::WorldTransformUpdatePassInfo worldTransformUpdatePassInfo{
                .worldTransformBufferAddress = m_worldTransformBufferAddresses[renderIndices.frameIndex],
                .worldTransformBuffer = *m_worldTransformBuffers[renderIndices.frameIndex],
                .nodeLevelRanges = scene.hostSceneData.levels};

            m_worldTransformUpdatePass.writeRenderCommands(
                cmd,
                worldTransformUpdatePassInfo);

            vk::BufferMemoryBarrier2 worldTransformToMeshInstanceCountBufferBarrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
                .srcAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                .dstStageMask = render::MeshInstancesCountPass::
                    clearIndirectDrawCommandsBuffer.stageFlags,
                .dstAccessMask = render::MeshInstancesCountPass::
                    clearIndirectDrawCommandsBuffer.accessFlags,
                .buffer = *m_worldTransformBuffers[renderIndices.frameIndex],
                .offset = 0,
                .size = vk::WholeSize};

            cmd.pipelineBarrier2({
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers =
                    &worldTransformToMeshInstanceCountBufferBarrier});

            render::MeshInstancesCountPassInfo meshInstancesCountPassInfo{
                .indirectDrawCommandsBufferAddress = m_indirectDrawCommandsBufferAddresses[renderIndices.frameIndex],
                .indirectDrawCommandsBuffer = *m_indirectDrawCommandsBuffers[renderIndices.frameIndex],
                .drawableCount = scene.sceneFrameRequirements.drawableObjectCount,
                .meshCount = scene.sceneFrameRequirements.meshCount,
                .clearIndirectDrawCommandsBuffer = true};

            m_meshInstanceCountPass.writeRenderCommands(
                cmd,
                meshInstancesCountPassInfo);

            std::array countToPrefixSumBufferBarriers{
                vk::BufferMemoryBarrier2{
                    .srcStageMask = render::MeshInstancesCountPass::
                        outputIndirectDrawCommandsBuffer.stageFlags,
                    .srcAccessMask = render::MeshInstancesCountPass::
                        outputIndirectDrawCommandsBuffer.accessFlags,
                    .dstStageMask = render::PrefixSumPass::
                        inputIndirectDrawCommandsBuffer.stageFlags,
                    .dstAccessMask = render::PrefixSumPass::
                        inputIndirectDrawCommandsBuffer.accessFlags,
                    .buffer = *m_indirectDrawCommandsBuffers[renderIndices.frameIndex],
                    .offset = 0,
                    .size = vk::WholeSize}};

            cmd.pipelineBarrier2({
                .bufferMemoryBarrierCount = static_cast<uint32_t>(countToPrefixSumBufferBarriers.size()),
                .pBufferMemoryBarriers = countToPrefixSumBufferBarriers.data()});

            render::PrefixSumPassInfo prefixSumPassInfo{
                .indirectDrawCommandsBufferAddress = m_indirectDrawCommandsBufferAddresses[renderIndices.frameIndex],
                .indirectDrawCommandsBuffer = *m_indirectDrawCommandsBuffers[renderIndices.frameIndex],
            };

            m_prefixSumPass.writeRenderCommands(cmd, prefixSumPassInfo);

            std::array prefixSumToInstanceRemapBufferBarriers{
                vk::BufferMemoryBarrier2{
                    .srcStageMask = render::PrefixSumPass::
                        outputIndirectDrawCommandsBuffer.stageFlags,
                    .srcAccessMask = render::PrefixSumPass::
                        outputIndirectDrawCommandsBuffer.accessFlags,
                    .dstStageMask = render::InstanceRemapPass::
                        inputIndirectDrawCommandsBuffer.stageFlags,
                    .dstAccessMask = render::InstanceRemapPass::
                        inputIndirectDrawCommandsBuffer.accessFlags,
                    .buffer = *m_indirectDrawCommandsBuffers[renderIndices.frameIndex],
                    .offset = 0,
                    .size = vk::WholeSize}};

            cmd.pipelineBarrier2({
                .bufferMemoryBarrierCount = static_cast<uint32_t>(
                    prefixSumToInstanceRemapBufferBarriers.size()),
                .pBufferMemoryBarriers =
                    prefixSumToInstanceRemapBufferBarriers.data()});

            render::InstanceRemapPassInfo instanceRemapPassInfo{
                .indirectDrawCommandsBufferAddress =
                    m_indirectDrawCommandsBufferAddresses[renderIndices.frameIndex],
                .instanceRemapBufferAddress =
                    m_instanceRemapBufferAddresses[renderIndices.frameIndex],
                .meshInstanceCursorBufferAddress =
                    m_meshInstanceCursorBufferAddresses[renderIndices.frameIndex],
                .meshInstanceCursorBuffer =
                    *m_meshInstanceCursorBuffers[renderIndices.frameIndex],
                .drawableCount = scene.sceneFrameRequirements.drawableObjectCount,
                .meshCount = scene.sceneFrameRequirements.meshCount,
                .clearMeshInstanceCursorBuffer = true};

            m_instanceRemapPass.writeRenderCommands(cmd, instanceRemapPassInfo);

            std::array toMainRenderPassBufferMemoryBarriers{
                vk::BufferMemoryBarrier2{
                    .srcStageMask = render::InstanceRemapPass::
                        outputInstanceRemapBuffer.stageFlags,
                    .srcAccessMask = render::InstanceRemapPass::
                        outputInstanceRemapBuffer.accessFlags,
                    .dstStageMask = render::MainRenderPass::
                        inputInstanceRemapBuffer.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        inputInstanceRemapBuffer.accessFlags,
                    .buffer = *m_instanceRemapBuffers[renderIndices.frameIndex],
                    .offset = 0,
                    .size = vk::WholeSize},
                vk::BufferMemoryBarrier2{
                    .srcStageMask = render::PrefixSumPass::
                        outputIndirectDrawCommandsBuffer.stageFlags,
                    .srcAccessMask = render::PrefixSumPass::
                        outputIndirectDrawCommandsBuffer.accessFlags,
                    .dstStageMask = render::MainRenderPass::
                        inputIndirectDrawCommandsBuffer.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        inputIndirectDrawCommandsBuffer.accessFlags,
                    .buffer = *m_indirectDrawCommandsBuffers[renderIndices.frameIndex],
                    .offset = 0,
                    .size = vk::WholeSize},
                vk::BufferMemoryBarrier2{
                    .srcStageMask = render::WorldTransformUpdatePass::
                        outputWorldTransformBuffer.stageFlags,
                    .srcAccessMask = render::WorldTransformUpdatePass::
                        outputWorldTransformBuffer.accessFlags,
                    .dstStageMask = render::MainRenderPass::
                        inputWorldTransformBuffer.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        inputWorldTransformBuffer.accessFlags,
                    .buffer = *m_worldTransformBuffers[renderIndices.frameIndex],
                    .offset = 0,
                    .size = vk::WholeSize}};

            std::array toMainRenderPassImageMemoryBarriers{
                vk::ImageMemoryBarrier2{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = render::MainRenderPass::
                        colorAttachmentInput.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        colorAttachmentInput.accessFlags,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = *m_colorAttachmentOutputs[renderIndices.frameIndex].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1}},
                vk::ImageMemoryBarrier2{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = render::MainRenderPass::
                        depthAttachmentInput.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        depthAttachmentInput.accessFlags,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = *m_depthAttachmentOutputs[renderIndices.frameIndex].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1}},
                vk::ImageMemoryBarrier2{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = render::MainRenderPass::
                        colorAttachmentOutput.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        colorAttachmentOutput.accessFlags,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = *m_debugOutputs1[renderIndices.frameIndex].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1}},
                vk::ImageMemoryBarrier2{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = render::MainRenderPass::
                        colorAttachmentOutput.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        colorAttachmentOutput.accessFlags,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = *m_debugOutputs2[renderIndices.frameIndex].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1}},
                vk::ImageMemoryBarrier2{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = render::MainRenderPass::
                        colorAttachmentOutput.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        colorAttachmentOutput.accessFlags,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = *m_debugOutputs3[renderIndices.frameIndex].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1}},
                vk::ImageMemoryBarrier2{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = render::MainRenderPass::
                        colorAttachmentOutput.stageFlags,
                    .dstAccessMask = render::MainRenderPass::
                        colorAttachmentOutput.accessFlags,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = *m_debugOutputs4[renderIndices.frameIndex].image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1}}};

            cmd.pipelineBarrier2({
                .bufferMemoryBarrierCount = static_cast<uint32_t>(
                    toMainRenderPassBufferMemoryBarriers.size()),
                .pBufferMemoryBarriers =
                    toMainRenderPassBufferMemoryBarriers.data(),
                .imageMemoryBarrierCount = static_cast<uint32_t>(
                    toMainRenderPassImageMemoryBarriers.size()),
                .pImageMemoryBarriers =
                    toMainRenderPassImageMemoryBarriers.data()});

            m_mainPassSettingSystems[renderIndices.frameIndex].updateSettings(
                m_mainWindow.getMainPassSettings());

            render::MainRenderPassInfo mainRenderPassInfo{
                .instanceRemapBufferAddress =
                    m_instanceRemapBufferAddresses[renderIndices.frameIndex],
                .mainPassSettingsBufferAddress =
                    m_mainPassSettingSystems[renderIndices.frameIndex]
                        .getMainPassSettingsBufferAddress(),
                .worldTransformBufferAddress =
                    m_worldTransformBufferAddresses[renderIndices.frameIndex],
                .colorAttachment = *m_colorAttachmentOutputs[renderIndices.frameIndex].view,
                .depthAttachment = *m_depthAttachmentOutputs[renderIndices.frameIndex].view,
                .indirectDrawCommandsBuffer =
                    *m_indirectDrawCommandsBuffers[renderIndices.frameIndex],
                .debugModeEnable = m_mainWindow.isDebugModeEnabled(),
                .debugOutputsInfo =
                    {.debugOutput1Attachment =
                         *m_debugOutputs1[renderIndices.frameIndex].view,
                     .debugOutput2Attachment =
                         *m_debugOutputs2[renderIndices.frameIndex].view,
                     .debugOutput3Attachment =
                         *m_debugOutputs3[renderIndices.frameIndex].view,
                     .debugOutput4Attachment =
                         *m_debugOutputs4[renderIndices.frameIndex].view},
                .meshCount = scene.sceneFrameRequirements.meshCount};

            m_mainRenderPass.writeRenderCommands(
                cmd,
                mainRenderPassInfo,
                m_mainWindow.getViewportExtent());
        }

        std::array mainRenderPassToPresentImageBarriers{
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = m_activeResources.swapchain.images[imageIndex],
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = render::MainRenderPass::
                    colorAttachmentOutput.stageFlags,
                .srcAccessMask = render::MainRenderPass::
                    colorAttachmentOutput.accessFlags,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *m_colorAttachmentOutputs[renderIndices.frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = render::MainRenderPass::
                    colorAttachmentOutput.stageFlags,
                .srcAccessMask = render::MainRenderPass::
                    colorAttachmentOutput.accessFlags,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *m_debugOutputs1[renderIndices.frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = render::MainRenderPass::
                    colorAttachmentOutput.stageFlags,
                .srcAccessMask = render::MainRenderPass::
                    colorAttachmentOutput.accessFlags,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *m_debugOutputs2[renderIndices.frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = render::MainRenderPass::
                    colorAttachmentOutput.stageFlags,
                .srcAccessMask = render::MainRenderPass::
                    colorAttachmentOutput.accessFlags,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *m_debugOutputs3[renderIndices.frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}},
            vk::ImageMemoryBarrier2{
                .srcStageMask = render::MainRenderPass::
                    colorAttachmentOutput.stageFlags,
                .srcAccessMask = render::MainRenderPass::
                    colorAttachmentOutput.accessFlags,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = *m_debugOutputs4[renderIndices.frameIndex].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}}};

        uint32_t barrierCount = 1;
        if (m_hasFrameResources && !isResizeMode)
        {
            barrierCount =
                static_cast<uint32_t>(mainRenderPassToPresentImageBarriers.size());
        }

        cmd.pipelineBarrier2({
            .imageMemoryBarrierCount = barrierCount,
            .pImageMemoryBarriers = mainRenderPassToPresentImageBarriers.data()});

        render::UiPassInfo uiPassInfo{
            .colorAttachment = *m_activeResources.swapchain.imageViews[imageIndex],
            .extent = m_activeResources.swapchain.extent};

        render::UiPass::writeRenderCommands(cmd, uiPassInfo);

        std::array uiPassToPresentImageBarriers{
            vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eNone,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::ePresentSrcKHR,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = m_activeResources.swapchain.images[imageIndex],
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}}};

        cmd.pipelineBarrier2({
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = uiPassToPresentImageBarriers.data()});

        if (auto endResult = cmd.end(); endResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to end graphics command buffer");
        }

        auto submitRes = m_activeResources.frameManager.submitRenderCommands(
            m_graphicsQueue,
            cmd,
            renderIndices.frameIndex,
            imageIndex);

        if (submitRes != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Failed to submit command buffer to graphics queue");
        }
    }

    void Application::drawFrame(bool isResizeMode, float dt)
    {
        m_mainWindow.pollFileDialogs();
        m_mainWindow.setResizeMode(isResizeMode);

        auto const* scene = findActiveScene();
        auto const* environment = findActiveEnvironment();

        if (scene != nullptr && environment != nullptr && !m_hasFrameResources)
        {
            updateFrameData();
        }

        if (!isResizeMode)
        {
            m_cameraController.update(
                dt, m_mainWindow.getCameraMoveSpeed(),
                m_mainWindow.getCameraRotationSpeed());
        }

        auto [prepareResult, renderIndices] = prepareFrame();
        if (prepareResult == vk::Result::eNotReady) {
            return;
        }
        if (prepareResult == vk::Result::eSuboptimalKHR || prepareResult == vk::Result::eErrorOutOfDateKHR) {
            recreateAllResources();
            return;
        }
        if (prepareResult != vk::Result::eSuccess) {
            throw std::runtime_error(
                "Fatal: Failed to prepare frame: " + vk::to_string(prepareResult));
        }

        doFrameRender(renderIndices, *scene, *environment, isResizeMode);

        auto presentResult = m_activeResources.frameManager.present(
            m_graphicsQueue,
            *m_activeResources.swapchain.swapchain,
            renderIndices.imageIndex);

        if (presentResult == vk::Result::eSuboptimalKHR ||
            presentResult == vk::Result::eErrorOutOfDateKHR)
        {
            recreateAllResources();
        }
        else if (presentResult != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Fatal: Failed to present rendered image to queue");
        }
        else
        {
            m_swapchainImageLayouts[renderIndices.imageIndex] = vk::ImageLayout::ePresentSrcKHR;
        }
    }

    int Application::run()
    {
        std::cout << "[Run] Entering main render loop. Engine is green.\n";

        while (!m_platform.shouldQuit() && !m_window.shouldClose())
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            m_deltaTime = std::chrono::duration<float>(currentTime - m_lastTime).count();
            m_lastTime = currentTime;
            m_totalTime += m_deltaTime;

            if (!m_platform.pollEvents()) break;
            if (m_window.isMinimized()) continue;
            drawFrame(false, m_deltaTime);
        }

        return 0;
    }

} // namespace shuttle::engine
