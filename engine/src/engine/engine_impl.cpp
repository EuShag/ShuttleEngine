//
// Created by Shagu on 05.09.2026.
//

#include "engine_impl.hpp"

#include <fstream>

#include "VkBootstrap.h"
#include "application/application.hpp"
#include "vulkan_debugger/vulkan_debugger.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace shuttle::engine {

    void SceneDeleter::operator()(SceneHandle sceneHandle) const {
        if (sceneHandle != NullHandle) {
            engine->destroyScene(sceneHandle);
        }
    }
    void EnvironmentDeleter::operator()(EnvironmentHandle environmentHandle) const {
        if (environmentHandle != NullHandle) {
            engine->destroyEnvironment(environmentHandle);
        }
    }
    void FrameResourcesDeleter::operator()(FrameResourcesHandle frameResourcesHandle) const {
        if (frameResourcesHandle != NullHandle) {
            engine->destroyFrameResources(frameResourcesHandle);
        }
    }
    void ViewportResourcesDeleter::operator()(ViewportResourcesHandle viewportResourcesHandle) const {
        if (viewportResourcesHandle != NullHandle) {
            engine->destroyViewportResources(viewportResourcesHandle);
        }
    }

    void SwapchainResourcesDeleter::operator()(SwapchainResourcesHandle swapchainResourcesHandle) const {
        if (swapchainResourcesHandle != NullHandle) {
            engine->destroySwapchainResources(swapchainResourcesHandle);
        }
    }

    void SurfaceDeleter::operator()(SurfaceHandle surfaceHandle) const {
        if (surfaceHandle != NullHandle) {
            engine->destroySurface(surfaceHandle);
        }
    }


    void ImUiContextDeleter::operator()(ImUiContextHandle imUiContextHandle) const {
        if (imUiContextHandle != NullHandle) {
            engine->destroyImUiContext(imUiContextHandle);
        }
    }

    vk::ResultValue<EngineImpl> EngineImpl::create() {
        m_dispatcher.init(m_dynamicLoader);

        // Получаем расширения поверхности от платформенного backend'а.
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
        m_dispatcher.init(instance);

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

        VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchainMaintenance1Features =
            vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR{
                .swapchainMaintenance1 = vk::True};

        auto vkbPhysicalDeviceResult =
            vkb::PhysicalDeviceSelector{instanceResult.value()}
                .set_minimum_version(1, 4)
                .add_required_extension(vk::KHRSwapchainExtensionName)
                .add_required_extension(vk::KHRSwapchainMaintenance1ExtensionName)
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
                .add_pNext(&swapchainMaintenance1Features)
                .build();

        if (!vkbDeviceResult.has_value())
        {
            throw std::runtime_error(
                "Failed to create logical device: " +
                vkbDeviceResult.error().message());
        }

        std::cout << "[System] Vulkan logical device created successfully.\n";

        auto graphicsQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::graphics);

        if (!graphicsQueueResult.has_value())
        {
            throw std::runtime_error("Failed to retrieve required queues from the logical device");
        }

        m_graphicsQueue = graphicsQueueResult.value().first;
        m_graphicsQueueFamilyIndex = graphicsQueueResult.value().second;
        m_physicalDevice = vkbPhysicalDeviceResult.value().physical_device;
        m_device = vkbDeviceResult.value().device;

        m_dispatcher.init(m_device);
        m_uniqueDevice = vk::UniqueDevice{
            m_device,
            vk::UniqueHandleTraits<
                vk::Device,
                vk::detail::DispatchLoaderDynamic>::deleter{
                nullptr,
                m_dispatcher}};

        auto [createAllocatorResult, uniqueAllocator] =
            resources::UniqueAllocator::makeUnique(
                *m_uniqueInstance,
                *m_uniqueDevice,
                m_physicalDevice,
                m_dispatcher);

        if (createAllocatorResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create VMA allocator");
        }

        m_allocator = std::move(uniqueAllocator);

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
                m_dispatcher, render::DescriptorHeapSetCreateInfo{
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

        auto createFallbackTexturesResult = initFallbackTextures();
        if (createFallbackTexturesResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create fallback textures");
        }

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

    }

    EngineImpl::~EngineImpl() {
        if (auto result = m_uniqueDevice->waitIdle(); result != vk::Result::eSuccess) {
            std::string message = "Failed to wait for device idle in destructor: " + vk::to_string(result) + "\n";
            assert(false && message.c_str());
            std::terminate();
        }
    }

    vk::Result EngineImpl::initFrameGraph(
        vk::Format colorAttachmentFormat,
        vk::Format depthAttachmentFormat
    ) {
        auto [createWorldTransformUpdatePipelineResult, worldTransformUpdatePipeline] =
            createComputePipeline("shaders/world_transform_update.comp.spv");

        if (createWorldTransformUpdatePipelineResult != vk::Result::eSuccess)
        {
            return createWorldTransformUpdatePipelineResult;
        }

        auto [createMeshInstanceCountPipelineResult, meshInstanceCountPipeline] =
            createComputePipeline("shaders/mesh_instances_count.comp.spv");

        if (createMeshInstanceCountPipelineResult != vk::Result::eSuccess)
        {
            return createMeshInstanceCountPipelineResult;
        }

        auto [createPrefixSumPipelineResult, prefixSumPipeline] =
            createComputePipeline("shaders/prefix_sum_pass.comp.spv");

        if (createPrefixSumPipelineResult != vk::Result::eSuccess)
        {
            return createPrefixSumPipelineResult;
        }

        auto [createInstanceRemapPipelineResult, instanceRemapPipeline] =
            createComputePipeline("shaders/instance_remap_pass.comp.spv");

        if (createInstanceRemapPipelineResult != vk::Result::eSuccess)
        {
            return createInstanceRemapPipelineResult;
        }

        auto [createDebugMainPipelineResult, debugMainPipeline] = createMainRenderPipeline(
            colorAttachmentFormat,
            depthAttachmentFormat,
            true
        );
        if (createDebugMainPipelineResult != vk::Result::eSuccess) return createDebugMainPipelineResult;
        auto [createMainPipelineResult, mainPipeline] = createMainRenderPipeline(
             colorAttachmentFormat,
             depthAttachmentFormat,
             false
        );
        if (createMainPipelineResult != vk::Result::eSuccess) return createMainPipelineResult;
        auto [createDebugSkyboxPipelineResult, debugSkyboxPipeline] = createSkyboxPipeline(
            colorAttachmentFormat,
            depthAttachmentFormat,
            true
        );
        if (createDebugSkyboxPipelineResult != vk::Result::eSuccess) return createDebugSkyboxPipelineResult;
        auto [createSkyboxPipelineResult, skyboxPipeline] = createSkyboxPipeline(
            colorAttachmentFormat,
            depthAttachmentFormat,
            false
        );
        if (createSkyboxPipelineResult != vk::Result::eSuccess) return createSkyboxPipelineResult;

        frameGraph = FrameGraph{
            .worldTransformUpdatePassPipeline = std::move(worldTransformUpdatePipeline),
            .meshInstanceCountPassPipeline = std::move(meshInstanceCountPipeline),
            .prefixSumPassPipeline = std::move(prefixSumPipeline),
            .instanceRemapPassPipeline = std::move(instanceRemapPipeline),
            .mainPassPipeline = std::move(mainPipeline),
            .skyboxPipeline = std::move(skyboxPipeline),
            .mainPassDebugPipeline = std::move(debugMainPipeline),
            .skyboxDebugPipeline = std::move(debugSkyboxPipeline)
        };
        return vk::Result::eSuccess;
    }

    vk::ResultValue<FrameResourcesHandle> EngineImpl::createFrameResources(
        SceneHandle sceneHandle,
        FrameResourcesHandle oldResources) {

        auto& scene = *m_uploadedScenes.get(sceneHandle);
        FrameResources frameResources;

        frameResources.m_cameraSystems.reserve(m_frameCount);
        frameResources.m_mainPassSettingSystems.reserve(m_frameCount);
        frameResources.m_worldTransformBuffers.reserve(m_frameCount);
        frameResources.m_instanceRemapBuffers.reserve(m_frameCount);
        frameResources.m_meshInstanceCursorBuffers.reserve(m_frameCount);
        frameResources.m_indirectDrawCommandsBuffers.reserve(m_frameCount);
        frameResources.m_worldTransformBufferAddresses.reserve(m_frameCount);
        frameResources.m_instanceRemapBufferAddresses.reserve(m_frameCount);
        frameResources.m_meshInstanceCursorBufferAddresses.reserve(m_frameCount);
        frameResources.m_indirectDrawCommandsBufferAddresses.reserve(m_frameCount);
        frameResources.frameStatuses.reserve(m_frameCount);
        frameResources.uniqueImageAvailableSemaphores.reserve(m_frameCount);

        for (int i = 0; i < m_frameCount; i++)
        {
            auto [createWorldTransformBufferResult, worldTransformBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(glm::mat4) * scene.hostSceneData.nodes.size(),
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createWorldTransformBufferResult != vk::Result::eSuccess)
            {
                return {createWorldTransformBufferResult, {}};
            }

            auto [createInstanceRemapBufferResult, instanceRemapBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(uint32_t) *
                                scene.hostSceneData.drawableObjects.size(),
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createInstanceRemapBufferResult != vk::Result::eSuccess)
            {
                return {createInstanceRemapBufferResult, {}};
            }

            auto [createMeshInstanceCursorBufferResult, meshInstanceCursorBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(uint32_t) *
                                scene.sceneFrameRequirements.meshCount,
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createMeshInstanceCursorBufferResult != vk::Result::eSuccess)
            {
                return {createMeshInstanceCursorBufferResult, {}};
            }

            auto [createIndirectDrawCommandsBufferResult, indirectDrawCommandsBuffer] =
                m_allocator->createAndAllocateBufferUnique(
                    vk::BufferCreateInfo{
                        .size = sizeof(vk::DrawIndexedIndirectCommand) *
                                scene.sceneFrameRequirements.meshCount,
                        .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eIndirectBuffer,
                        .sharingMode = vk::SharingMode::eExclusive},
                    resources::MemoryUsage::eGpuOnly);

            if (createIndirectDrawCommandsBufferResult != vk::Result::eSuccess)
            {
                return {createIndirectDrawCommandsBufferResult, {}};
            }

            auto [createCameraSystemResult, m_cameraSystem] =
                render::CameraSystem::create(*m_uniqueDevice, m_dispatcher, *m_allocator);

            if (createCameraSystemResult != vk::Result::eSuccess)
            {
                return {createCameraSystemResult, {}};
            }

            auto [createMainPassSettingSystemResult, mainPassSettingSystem] =
                render::MainPassSettingSystem::create(*m_uniqueDevice, m_dispatcher, *m_allocator);

            if (createMainPassSettingSystemResult != vk::Result::eSuccess)
            {
                return {createMainPassSettingSystemResult, {}};
            }

            auto [createSemaphoreResult, semaphore] = m_uniqueDevice->createSemaphoreUnique({}, nullptr, m_dispatcher);


            frameResources.m_cameraSystems.push_back(std::move(m_cameraSystem));
            frameResources.m_mainPassSettingSystems.push_back(std::move(mainPassSettingSystem));
            frameResources.m_worldTransformBuffers.push_back(std::move(worldTransformBuffer));
            frameResources.m_instanceRemapBuffers.push_back(std::move(instanceRemapBuffer));
            frameResources.m_meshInstanceCursorBuffers.push_back(std::move(meshInstanceCursorBuffer));
            frameResources.m_indirectDrawCommandsBuffers.push_back(std::move(indirectDrawCommandsBuffer));
            frameResources.m_worldTransformBufferAddresses.push_back(m_uniqueDevice->getBufferAddress({.buffer = *frameResources.m_worldTransformBuffers.back()}));
            frameResources.m_instanceRemapBufferAddresses.push_back(m_uniqueDevice->getBufferAddress({.buffer = *frameResources.m_instanceRemapBuffers.back()}));
            frameResources.m_meshInstanceCursorBufferAddresses.push_back(m_uniqueDevice->getBufferAddress({.buffer = *frameResources.m_meshInstanceCursorBuffers.back()}));
            frameResources.m_indirectDrawCommandsBufferAddresses.push_back(m_uniqueDevice->getBufferAddress({.buffer = *frameResources.m_indirectDrawCommandsBuffers.back()}));
            frameResources.frameStatuses.push_back(vk::Result::eSuccess);
            frameResources.uniqueImageAvailableSemaphores.push_back(std::move(semaphore));
        }

        auto handle = m_frameResources.allocate(std::move(frameResources));

        m_frameResources.markForDeletion(oldResources);

        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<ViewportResourcesHandle> EngineImpl::createViewportResources(vk::Extent2D viewportExtent) {

        ViewportResources viewportResources;

        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            auto [createColorAttachmentOutputResult, colorAttachmentOutput] =
                createAttachmentOutput(
                    *m_uniqueDevice,
                    m_dispatcher,
                    *m_allocator,
                    {
                        .format = vk::Format::eB8G8R8A8Srgb,
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
                    *m_uniqueDevice,
                    m_dispatcher,
                    *m_allocator,
                    {
                        .format = vk::Format::eD32Sfloat,
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
                        *m_uniqueDevice,
                        m_dispatcher,
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

            viewportResources.m_debugOutputs1[i] = std::move(createDebugOutputResources());
            viewportResources.m_debugOutputs2[i] = std::move(createDebugOutputResources());
            viewportResources.m_debugOutputs3[i] = std::move(createDebugOutputResources());
            viewportResources.m_debugOutputs4[i] = std::move(createDebugOutputResources());

            viewportResources.m_colorAttachmentOutputs[i] = std::move(colorAttachmentOutput);
            viewportResources.m_depthAttachmentOutputs[i] = std::move(depthAttachmentOutput);
        }

        viewportResources.extent = viewportExtent;

        auto handle = m_viewportResources.allocate(std::move(viewportResources));
        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<ViewportIndices> EngineImpl::getViewportIndices(
        ImUiContextHandle imUiContextHandle,
        ViewportTexturesHandle viewportTexturesHandle,
        uint32_t index) {

        auto &imUiContext = *m_imGuiContexts.get(imUiContextHandle);

        return imUiContext.getViewportIndices(viewportTexturesHandle, index);
    }

    vk::ResultValue<ViewportTexturesHandle> EngineImpl::createViewportTextures(
        ImUiContextHandle imUiContextHandle,
        ViewportResourcesHandle viewportResourcesHandle) {
        auto &imUiContext = *m_imGuiContexts.get(imUiContextHandle);
        auto const& viewportResources = *m_viewportResources.get(viewportResourcesHandle);

        return imUiContext.createViewportTextures(viewportResources);
    }

    vk::ResultValue<SurfaceHandle> EngineImpl::createSurface(
    pal::Platform const& platform,
    pal::WindowHandle const& window)
    {
        // 1. Создаем нативную поверхностность Vulkan через платформу
        auto [createSurfaceResult, surface] = createVulkanSurfaceUnique(
            *m_uniqueInstance,
            platform,
            window,
            m_dispatcher
        );

        if (createSurfaceResult != vk::Result::eSuccess) {
            return {createSurfaceResult, {}};
        }

        // 2. Выделяем слот через SlotMapStorage:
        //    • Достает свободный индекс из Lock-Free очереди (или создает новый)
        //    • Упаковывает текущее поколение слота + индекс в 64-битный Handle
        SurfaceHandle handle = m_surfaces.allocate(std::move(surface));

        return {vk::Result::eSuccess, {handle}};
    }

    void EngineImpl::destroyScene(SceneHandle sceneHandle) {
        m_uploadedScenes.markForDeletion(sceneHandle);
    }

    void EngineImpl::destroyEnvironment(EnvironmentHandle environmentHandle) {
        m_environmentResources.markForDeletion(environmentHandle);
    }

    void EngineImpl::destroyFrameResources(FrameResourcesHandle frameResourcesHandle) {
        m_frameResources.markForDeletion(frameResourcesHandle);
    }

    void EngineImpl::destroyViewportResources(ViewportResourcesHandle viewportResourcesHandle) {
        m_viewportResources.markForDeletion(viewportResourcesHandle);
    }

    void EngineImpl::destroySurface(SurfaceHandle surfaceHandle) {
        m_surfaces.markForDeletion(surfaceHandle);
    }

    void EngineImpl::destroySwapchainResources(SwapchainResourcesHandle swapchainResourcesHandle) {
        m_swapchainResources.markForDeletion(swapchainResourcesHandle);
    }

    void EngineImpl::destroyImUiContext(ImUiContextHandle imUiContextHandle) {
        m_imGuiContexts.markForDeletion(imUiContextHandle);
    }

    vk::ResultValue<vk::UniqueShaderModule> EngineImpl::createShaderModuleUnique(std::filesystem::path const &shaderPath) const {
        std::fstream shaderModuleFile(shaderPath, std::ios::binary | std::ios::ate | std::ios::in);
        if (!shaderModuleFile.is_open())
        {
            throw std::runtime_error("Failed to open fragment shader file");
        }
        auto const shaderModuleFileSize = shaderModuleFile.tellg();
        std::vector<char> fragmentShaderCode(shaderModuleFileSize);
        shaderModuleFile.seekg(0);
        shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

        return m_uniqueDevice->createShaderModuleUnique({
            .codeSize = fragmentShaderCode.size(),
            .pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())},
            nullptr,
            m_dispatcher
        );
    }

    void EngineImpl::writeCopyBufferCommand(
            vk::CommandBuffer commandBuffer,
            vk::Buffer stagingBuffer,
            std::vector<CopyBufferCommandInfo> const& copyBufferInfos
        ) const {

        for (auto const& copyCommandBufferInfo : copyBufferInfos)
        {

            vk::BufferCopy2 region{
                .srcOffset = copyCommandBufferInfo.stagingOffset,
                .dstOffset = 0,
                .size = copyCommandBufferInfo.dataSize
            };

            vk::CopyBufferInfo2 copyBufferInfo{
                .srcBuffer = stagingBuffer,
                .dstBuffer = copyCommandBufferInfo.destinationBuffer,
                .regionCount = 1,
                .pRegions = &region
            };

            if (copyCommandBufferInfo.dataSize == 0) continue;
            commandBuffer.copyBuffer2(copyBufferInfo, m_dispatcher);
        }
    }

    void EngineImpl::writeImageSourceBarrier(
        vk::CommandBuffer commandBuffer,
        std::vector<ImageBarrierInfo> const &textures
    ) const {
        std::vector<vk::ImageMemoryBarrier2> textureBarriers(textures.size());

        for (uint32_t i = 0; i < textureBarriers.size(); ++i)
        {
            textureBarriers[i] = {.srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = textures[i].image,
                .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = textures[i].mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1}};
        }

        commandBuffer.pipelineBarrier2({
            .imageMemoryBarrierCount = static_cast<uint32_t>(textureBarriers.size()),
            .pImageMemoryBarriers = textureBarriers.data()},
            m_dispatcher
        );
    }

    void EngineImpl::writeBufferAndImageDestinationBarrier(
        vk::CommandBuffer commandBuffer,
        std::vector<ImageBarrierInfo> const &textures,
        std::vector<BufferBarrierInfo> const &buffers
    ) const {

        std::vector<vk::ImageMemoryBarrier2> textureBarriers(textures.size());
        std::vector<vk::BufferMemoryBarrier2> bufferBarriers(buffers.size());

        for (uint32_t i = 0; i < textureBarriers.size(); ++i)
        {
            textureBarriers[i] = {
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eNone,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = textures[i].image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = textures[i].mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1}};
        }
        for (uint32_t i = 0; i < buffers.size(); ++i)
        {
            bufferBarriers[i] = {
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eNone,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .buffer = buffers[i].buffer,
                .offset = 0,
                .size = buffers[i].size,
            };
        }

        commandBuffer.pipelineBarrier2({
            .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
            .pBufferMemoryBarriers = bufferBarriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(textureBarriers.size()),
            .pImageMemoryBarriers = textureBarriers.data()},
            m_dispatcher
        );
    }

    void EngineImpl::writeCopyBufferImageCommands(
        vk::CommandBuffer commandBuffer,
        vk::Buffer stagingBuffer,
        const std::vector<vk::Image> &images,
        std::vector<ImageMipUploadInfo> const &textureMipUploadInfos) const {

        std::vector<std::vector<vk::BufferImageCopy2>> bufferImageCopies(images.size());
        std::vector<vk::CopyBufferToImageInfo2> copyBufferToImageInfos(images.size());

        for (auto const& textureMipUploadInfo : textureMipUploadInfos)
        {
            bufferImageCopies[textureMipUploadInfo.imageIndex].push_back(
                {.bufferOffset = textureMipUploadInfo.stagingOffset,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = textureMipUploadInfo.mipLevel,
                        .baseArrayLayer = 0,
                        .layerCount = 1},
                    .imageOffset = {.x = 0, .y = 0, .z = 0},
                    .imageExtent = {
                        .width = textureMipUploadInfo.width,
                        .height = textureMipUploadInfo.height,
                        .depth = 1,
                    }});
        }
        for (auto i = 0; i < copyBufferToImageInfos.size(); ++i)
        {

            copyBufferToImageInfos[i] = vk::CopyBufferToImageInfo2{
                .srcBuffer = stagingBuffer,
                .dstImage = images[i],
                .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                .regionCount = static_cast<uint32_t>(bufferImageCopies[i].size()),
                .pRegions = bufferImageCopies[i].data()};
            if (copyBufferToImageInfos[i].regionCount == 0) continue;

            commandBuffer.copyBufferToImage2(copyBufferToImageInfos[i], m_dispatcher);
        }
    }

    vk::ResultValue<vk::UniqueCommandBuffer> EngineImpl::allocateOneTimeCommandBuffer() {
        auto [allocateCommandBufferResult, uniqueCommandBuffers] = m_uniqueDevice->allocateCommandBuffersUnique({
            .commandPool = *m_uniqueGraphicsCommandPool,
            .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1},
            m_dispatcher
        );
        return {allocateCommandBufferResult, std::move(uniqueCommandBuffers[0])};
    }

    vk::ResultValue<render::Texture> EngineImpl::createTexture(
        vk::Format format,
        vk::ImageViewType imageViewType,
        vk::Extent3D size,
        uint32_t mipLevels,
        uint32_t arrayLayers) {

        bool cubeMap = imageViewType == vk::ImageViewType::eCube || imageViewType == vk::ImageViewType::eCubeArray;

        vk::ImageType imageType = vk::ImageType::e2D;

        switch (imageViewType) {
            case vk::ImageViewType::eCube:
            case vk::ImageViewType::eCubeArray:
            case vk::ImageViewType::e2D:
            case vk::ImageViewType::e2DArray:
                imageType = vk::ImageType::e2D; break;
            case vk::ImageViewType::e3D:
                imageType = vk::ImageType::e3D; break;
            case vk::ImageViewType::e1D:
            case vk::ImageViewType::e1DArray:
                imageType = vk::ImageType::e1D; break;
        }

        auto [createImageResult, image] = m_allocator->createAndAllocateImageUnique(
            {
                .flags = cubeMap ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{},
                .imageType = imageType,
                .format = format,
                .extent = size,
                .mipLevels = mipLevels,
                .arrayLayers = arrayLayers,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
            },
            resources::MemoryUsage::eGpuOnly
        );
        if (createImageResult != vk::Result::eSuccess) return {createImageResult, {}};

        auto [createImageViewResult, imageView] = m_uniqueDevice->createImageViewUnique(
            {
                .image = *image,
                .viewType = imageViewType,
                .format = format,
                .components = {
                    .r = vk::ComponentSwizzle::eR,
                    .g = vk::ComponentSwizzle::eG,
                    .b = vk::ComponentSwizzle::eB,
                    .a = vk::ComponentSwizzle::eA,
                },
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = arrayLayers
                }
            }, nullptr, m_dispatcher
        );
        if (createImageViewResult != vk::Result::eSuccess) return {createImageViewResult, {}};

        auto [createTextureResult, descriptorSlot] = m_descriptorHeapSet.writeTextureUnique(*imageView);
        if (createTextureResult != vk::Result::eSuccess) return {createTextureResult, {}};

        return {vk::Result::eSuccess, {
            .image = std::move(image),
            .imageView = std::move(imageView),
            .descriptorSlot = std::move(descriptorSlot),
        }};
    }

    vk::ResultValue<resources::UniqueAllocatedBuffer> EngineImpl::createShaderDeviceAddressBuffer(vk::DeviceSize size) {
        return m_allocator->createAndAllocateBufferUnique({
                .size = size == 0 ? 16 : size,
                .usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive},
            resources::MemoryUsage::eGpuOnly);
    }

    vk::ResultValue<resources::UniqueAllocatedBuffer> EngineImpl::createStagingBuffer(vk::DeviceSize size) {
        return m_allocator->createAndAllocateBufferUnique({
            .size = size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive},
        resources::MemoryUsage::eCpuToGpu, resources::AllocationCreateFlagBits::eMapped);
    }

    vk::ResultValue<resources::UniqueAllocatedBuffer> EngineImpl::createIndexBuffer(vk::DeviceSize size) {
        return m_allocator->createAndAllocateBufferUnique(
            vk::BufferCreateInfo{.size = size == 0 ? 16 : size,
                .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                         vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive},
            resources::MemoryUsage::eGpuOnly);
    }

    vk::Result EngineImpl::submitTransferAndWait(vk::CommandBuffer commandBuffer) const {
        vk::CommandBufferSubmitInfo cmdSubmitInfo = {
            .commandBuffer = commandBuffer
        };

        auto [fenceCreateResult, fence] = m_uniqueDevice->createFenceUnique(
            {}, nullptr, m_dispatcher
        );
        if (fenceCreateResult != vk::Result::eSuccess) return fenceCreateResult;

        auto const submitResult = m_graphicsQueue.submit2({vk::SubmitInfo2{
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdSubmitInfo
        }}, *fence, m_dispatcher);

        if (submitResult != vk::Result::eSuccess) return submitResult;

        return waitForAllFences({*fence});
    }

    vk::Result EngineImpl::getFenceStatus(vk::Fence fence) const {
        return m_uniqueDevice->getFenceStatus(fence);
    }

    vk::Result EngineImpl::waitForAnyFence(std::initializer_list<vk::Fence> fences, uint64_t timeout) const {
        return m_uniqueDevice->waitForFences(fences, vk::False, timeout, m_dispatcher);
    }
    vk::Result EngineImpl::waitForAllFences(std::initializer_list<vk::Fence> fences, uint64_t timeout) const {
        return m_uniqueDevice->waitForFences(fences, vk::True, timeout, m_dispatcher);
    }

    vk::Result EngineImpl::waitForAnyFence(std::vector<vk::Fence> const& fences, uint64_t timeout) const {
        return m_uniqueDevice->waitForFences(fences, vk::False, timeout, m_dispatcher);
    }
    vk::Result EngineImpl::waitForAllFences(std::vector<vk::Fence> const& fences, uint64_t timeout) const {
        return m_uniqueDevice->waitForFences(fences, vk::True, timeout, m_dispatcher);
    }
}
