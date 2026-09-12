//
// Created by Shagu on 05.09.2026.
//

#include "Engine.hpp"

#include <fstream>

#include "Application/Application.hpp"
#include "VulkanDebugger/VulkanDebugger.hpp"

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
    void FrameGraphDeleter::operator()(FrameGraphHandle frameGraphHandle) const {
        if (frameGraphHandle != NullHandle) {
            engine->destroyFrameGraph(frameGraphHandle);
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


    Engine::Engine(
        pal::Platform&  platform,
        pal::WindowHandle& windowHandle,
        pal::WindowBase& window
    ) {
        m_dispatcher.init(m_dynamicLoader);

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

        // Создаем Vulkan Surface через абстракцию нашей Платформы
        auto [result, uniqueSurface] = createVulkanSurfaceUnique(*m_uniqueInstance, platform, windowHandle);

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

        VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchainMaintenance1Features =
            vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR{
                .swapchainMaintenance1 = vk::True};

        auto vkbPhysicalDeviceResult =
            vkb::PhysicalDeviceSelector{instanceResult.value(), *m_uniqueSurface}
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

        // Создаем ImGuiContext через универсальный WindowBase (m_window)
        auto [uiRenderResult, uiRenderObj] =
            ImGuiContextM::create(
                window,
                instance,
                m_physicalDevice,
                m_device,
                m_graphicsQueueFamilyIndex,
                m_graphicsQueue,
                swapchain.imageCount);

        if (uiRenderResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create ImGuiContext");
        }

        render::MainPassSettings mainPassSettings{};
        m_imGuiContext = std::move(uiRenderObj);

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
            .swapchainManager = std::move(frameManager)};
        m_swapchainImageLayouts.resize(
            m_activeResources.swapchain.imageCount,
            vk::ImageLayout::eUndefined);

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

    Engine::~Engine() {
        if (auto result = m_device.waitIdle(); result != vk::Result::eSuccess) {
            std::string message = "Failed to wait for device idle in destructor: " + vk::to_string(result) + "\n";
            assert(false && message.c_str());
            std::terminate();
        }
    }

    vk::ResultValue<Engine::RenderIndices> Engine::prepareFrame(
        FrameResourcesHandle frameResources,
        SwapchainResourcesHandle swapchain
    ) {
        auto [prepareRes, currentFrameIndex] = m_activeResources.swapchainManager.acquireFrameSlot(m_device);

        if (prepareRes != vk::Result::eSuccess) {
            return {prepareRes, {}};
        }
        m_retireController.renderRetireUpdate(currentFrameIndex);
        m_resourceBin.release(currentFrameIndex);
        auto [acquireResult, currentImageIndex] = m_activeResources.swapchainManager.acquireNextImage(
            m_device,
            *m_activeResources.swapchain.swapchain,
            currentFrameIndex);
        if (acquireResult != vk::Result::eSuccess) {
            return {acquireResult, {}};
        }
        m_retireController.presentRetireUpdate(currentImageIndex);
        return {vk::Result::eSuccess, {.frameIndex = currentFrameIndex, .imageIndex = currentImageIndex}};
    }

    vk::ResultValue<EnvironmentHandle> Engine::createEnvironmentResources(
        std::filesystem::path const &filePath) {
        auto [result, environment] = render::createEnvironmentResources(
            m_renderContext,
            m_graphicsQueue,
            *m_uniqueGraphicsCommandPool,
            m_descriptorHeapSet,
            filePath);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        auto handle = m_environmentResources.allocate(std::move(environment));
        return {result, handle};
    }

    vk::ResultValue<SceneHandle> Engine::uploadScene(render::LoadedSceneData const &sceneData) {
        auto [result, scene] = render::uploadScene(
            sceneData,
            m_renderContext,
            m_graphicsQueue,
            *m_uniqueGraphicsCommandPool,
            m_descriptorHeapSet,
            m_fallbackTextures.indices);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        auto handle = m_uploadedScenes.allocate(std::move(scene));
        return {result, handle};
    }

    vk::ResultValue<UniqueSceneHandle> Engine::uploadSceneUnique(render::LoadedSceneData const &sceneData) {
        auto [result, sceneHandle] = uploadScene(sceneData);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueSceneHandle{sceneHandle, SceneDeleter{this}}};
    }

    vk::ResultValue<UniqueEnvironmentHandle> Engine::createEnvironmentUnique(std::filesystem::path const &filePath) {
        auto [result, environmentHandle] = createEnvironmentResources(filePath);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueEnvironmentHandle{environmentHandle, EnvironmentDeleter{this}}};
    }

    vk::ResultValue<FrameGraphHandle> Engine::createFrameGraph() {
        auto [createWorldTransformUpdatePassResult, worldTransformUpdatePass_] =
            render::WorldTransformUpdatePass::create(m_device, *m_pipelineLayout);

        if (createWorldTransformUpdatePassResult != vk::Result::eSuccess)
        {
            return {createWorldTransformUpdatePassResult, {}};
        }

        auto [createMeshInstanceCountPassResult, meshInstanceCountPass_] =
            render::MeshInstancesCountPass::create(m_device, *m_pipelineLayout);

        if (createMeshInstanceCountPassResult != vk::Result::eSuccess)
        {
            return {createMeshInstanceCountPassResult, {}};
        }

        auto [createPrefixSumPassResult, prefixSumPass_] =
            render::PrefixSumPass::create(m_device, *m_pipelineLayout);

        if (createPrefixSumPassResult != vk::Result::eSuccess)
        {
            return {createPrefixSumPassResult, {}};
        }

        auto [createInstanceRemapPassResult, instanceRemapPass_] =
            render::InstanceRemapPass::create(m_device, *m_pipelineLayout);

        if (createInstanceRemapPassResult != vk::Result::eSuccess)
        {
            return {createInstanceRemapPassResult, {}};
        }

        auto [createMainRenderPassResult, mainRenderPass_] =
            render::MainRenderPass::create(
                m_device,
                *m_pipelineLayout,
                m_renderContext.swapchainColorFormat,
                m_renderContext.swapchainDepthFormat);

        if (createMainRenderPassResult != vk::Result::eSuccess)
        {
            return {createMainRenderPassResult, {}};
        }

        auto handle = m_frameGraphs.allocate(
            FrameGraph{
                .m_worldTransformUpdatePass = std::move(worldTransformUpdatePass_),
                .m_meshInstanceCountPass = std::move(meshInstanceCountPass_),
                .m_prefixSumPass = std::move(prefixSumPass_),
                .m_instanceRemapPass = std::move(instanceRemapPass_),
                .m_mainRenderPass = std::move(mainRenderPass_)
            }
        );

        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<UniqueFrameGraphHandle> Engine::createFrameGraphUnique() {
        auto [result, frameGraphHandle] = createFrameGraph();

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueFrameGraphHandle{frameGraphHandle, FrameGraphDeleter{this}}};
    }

    vk::ResultValue<FrameResourcesHandle> Engine::createFrameResources(
        SceneHandle sceneHandle,
        FrameResourcesHandle oldResources) {

        auto& scene = *m_uploadedScenes.get(sceneHandle);
        FrameResources frameResources;

        auto [createFrameManagerResult, frameManager] =
            FrameManager::create(m_device, m_frameCount, m_dispatcher);
        if (createFrameManagerResult != vk::Result::eSuccess)
        {
            return {createFrameManagerResult, {}};
        }
        frameResources.m_frameManager = std::move(frameManager);

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
                render::CameraSystem::create(m_device, *m_allocator);

            if (createCameraSystemResult != vk::Result::eSuccess)
            {
                return {createCameraSystemResult, {}};
            }

            auto [createMainPassSettingSystemResult, mainPassSettingSystem] =
                render::MainPassSettingSystem::create(m_device, *m_allocator);

            if (createMainPassSettingSystemResult != vk::Result::eSuccess)
            {
                return {createMainPassSettingSystemResult, {}};
            }

            frameResources.m_cameraSystems.push_back(std::move(m_cameraSystem));
            frameResources.m_mainPassSettingSystems.push_back(std::move(mainPassSettingSystem));
            frameResources.m_worldTransformBuffers.push_back(std::move(worldTransformBuffer));
            frameResources.m_instanceRemapBuffers.push_back(std::move(instanceRemapBuffer));
            frameResources.m_meshInstanceCursorBuffers.push_back(std::move(meshInstanceCursorBuffer));
            frameResources.m_indirectDrawCommandsBuffers.push_back(std::move(indirectDrawCommandsBuffer));
            frameResources.m_worldTransformBufferAddresses.push_back(m_device.getBufferAddress({.buffer = *frameResources.m_worldTransformBuffers.back()}));
            frameResources.m_instanceRemapBufferAddresses.push_back(m_device.getBufferAddress({.buffer = *frameResources.m_instanceRemapBuffers.back()}));
            frameResources.m_meshInstanceCursorBufferAddresses.push_back(m_device.getBufferAddress({.buffer = *frameResources.m_meshInstanceCursorBuffers.back()}));
            frameResources.m_indirectDrawCommandsBufferAddresses.push_back(m_device.getBufferAddress({.buffer = *frameResources.m_indirectDrawCommandsBuffers.back()}));
        }

        auto handle = m_frameResources.allocate(std::move(frameResources));

        m_frameResources.markForDeletion(oldResources);

        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<UniqueFrameResourcesHandle> Engine::createFrameResourcesUnique(
        SceneHandle scene,
        FrameResourcesHandle oldResources) {
        auto [result, frameResourcesHandle] = createFrameResources(scene, oldResources);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueFrameResourcesHandle{frameResourcesHandle, FrameResourcesDeleter{this}}};
    }

    vk::ResultValue<ViewportResourcesHandle> Engine::createViewportResources(vk::Extent2D viewportExtent) {

        ViewportResources viewportResources;

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

            viewportResources.m_debugOutputs1[i] = std::move(createDebugOutputResources());
            viewportResources.m_debugOutputs2[i] = std::move(createDebugOutputResources());
            viewportResources.m_debugOutputs3[i] = std::move(createDebugOutputResources());
            viewportResources.m_debugOutputs4[i] = std::move(createDebugOutputResources());

            viewportResources.m_colorAttachmentOutputs[i] = std::move(colorAttachmentOutput);
            viewportResources.m_depthAttachmentOutputs[i] = std::move(depthAttachmentOutput);

            auto [createColorAttachmentSetResult, colorAttachmentSet] =
                m_imGuiContext.createImGuiTextureDescriptorSetUnique(
                    *viewportResources.m_colorAttachmentOutputs[i].view);
            if (createColorAttachmentSetResult != vk::Result::eSuccess)
            {
                return {createColorAttachmentOutputResult, {}};
            }

            auto [createDebugAttachmentSet1Result, debugAttachmentSet1] =
                m_imGuiContext.createImGuiTextureDescriptorSetUnique(
                    *viewportResources.m_debugOutputs1[i].view);
            if (createDebugAttachmentSet1Result != vk::Result::eSuccess)
            {
                return {createDebugAttachmentSet1Result, {}};
            }

            auto [createDebugAttachmentSet2Result, debugAttachmentSet2] =
                m_imGuiContext.createImGuiTextureDescriptorSetUnique(
                    *viewportResources.m_debugOutputs2[i].view);
            if (createDebugAttachmentSet2Result != vk::Result::eSuccess)
            {
                return {createDebugAttachmentSet2Result, {}};
            }

            auto [createDebugAttachmentSet3Result, debugAttachmentSet3] =
                m_imGuiContext.createImGuiTextureDescriptorSetUnique(
                    *viewportResources.m_debugOutputs3[i].view);
            if (createDebugAttachmentSet3Result != vk::Result::eSuccess)
            {
                return {createDebugAttachmentSet3Result, {}};
            }

            auto [createDebugAttachmentSet4Result, debugAttachmentSet4] =
                m_imGuiContext.createImGuiTextureDescriptorSetUnique(
                    *viewportResources.m_debugOutputs4[i].view);
            if (createDebugAttachmentSet4Result != vk::Result::eSuccess)
            {
                return {createDebugAttachmentSet4Result, {}};
            }

            viewportResources.m_colorAttachmentSets[i] = std::move(colorAttachmentSet);
            viewportResources.m_debugAttachmentSets1[i] = std::move(debugAttachmentSet1);
            viewportResources.m_debugAttachmentSets2[i] = std::move(debugAttachmentSet2);
            viewportResources.m_debugAttachmentSets3[i] = std::move(debugAttachmentSet3);
            viewportResources.m_debugAttachmentSets4[i] = std::move(debugAttachmentSet4);
        }

        auto handle = m_viewportResources.allocate(std::move(viewportResources));
        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<UniqueViewportResourcesHandle> Engine::createViewportResourcesUnique(vk::Extent2D viewportExtent) {
        auto [result, viewportResourcesHandle] = createViewportResources(viewportExtent);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueViewportResourcesHandle{viewportResourcesHandle, ViewportResourcesDeleter{this}}};
    }

    vk::ResultValue<ViewportIndices> Engine::getViewportIndices(
        ViewportResourcesHandle viewportResourcesHandle,
        uint32_t frameIndex) {

        auto &viewportHandle = *m_viewportResources.get(viewportResourcesHandle);

        if (frameIndex >= viewportHandle.m_colorAttachmentSets.size()) {
            return {vk::Result::eErrorOutOfHostMemory, {}};
        }

        return {vk::Result::eSuccess,
        {
            .releaseTextureIndex = reinterpret_cast<ImU64>(static_cast<VkDescriptorSet>(*viewportHandle.m_colorAttachmentSets[frameIndex])),
            .debug1TextureIndex = reinterpret_cast<ImU64>(static_cast<VkDescriptorSet>(*viewportHandle.m_debugAttachmentSets1[frameIndex])),
            .debug2TextureIndex = reinterpret_cast<ImU64>(static_cast<VkDescriptorSet>(*viewportHandle.m_debugAttachmentSets2[frameIndex])),
            .debug3TextureIndex = reinterpret_cast<ImU64>(static_cast<VkDescriptorSet>(*viewportHandle.m_debugAttachmentSets3[frameIndex])),
            .debug4TextureIndex = reinterpret_cast<ImU64>(static_cast<VkDescriptorSet>(*viewportHandle.m_debugAttachmentSets4[frameIndex]))
        }};

    }

    vk::ResultValue<SurfaceHandle> Engine::createSurface(
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


    vk::ResultValue<UniqueSurfaceHandle> Engine::createSurfaceUnique(pal::Platform &platform, pal::WindowHandle &window)
    {
        auto [result, surfaceHandle] = createSurface(platform, window);

        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueSurfaceHandle{surfaceHandle, {this}}};
    }

    vk::ResultValue<SwapchainResourcesHandle> Engine::createSwapchainResources(
        SurfaceHandle surfaceHandle,
        vk::Extent2D viewportExtent,
        SwapchainResourcesHandle oldSwapchainResourcesHandle) {

        SwapchainContext context {
            .physicalDevice = m_physicalDevice,
            .device = *m_uniqueDevice,
            .surface = **m_surfaces.get(surfaceHandle),
        };

        auto [createSwapchainResult, swapchain] =
            createSwapchain(context, viewportExtent, oldSwapchainResourcesHandle != NullHandle ? *m_swapchainResources.get(oldSwapchainResourcesHandle)->swapchain.swapchain : nullptr);

        if (createSwapchainResult != vk::Result::eSuccess)
        {
            return {createSwapchainResult, {}};
        }

        auto [createFrameManagerResult, swapchainManager] =
            SwapchainManager::create(m_device, swapchain.imageCount);

        if (createFrameManagerResult != vk::Result::eSuccess)
        {
            return {createFrameManagerResult, {}};
        }

        auto handle = m_swapchainResources.allocate(
            SwapchainResources{
                .swapchain = std::move(swapchain),
                .swapchainManager = std::move(swapchainManager)
                });

        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<UniqueSwapchainResourcesHandle> Engine::createSwapchainResourcesUnique(
        SurfaceHandle surfaceHandle,
        vk::Extent2D viewportExtent,
        SwapchainResourcesHandle oldSwapchainResourcesHandle) {

        auto [result, swapchainResourcesHandle] = createSwapchainResources(surfaceHandle, viewportExtent, oldSwapchainResourcesHandle);
        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        return {result, UniqueSwapchainResourcesHandle{swapchainResourcesHandle, SwapchainResourcesDeleter{this}}};
    }

    void Engine::destroyScene(SceneHandle sceneHandle) {
        m_uploadedScenes.markForDeletion(sceneHandle);
    }

    void Engine::destroyEnvironment(EnvironmentHandle environmentHandle) {
        m_environmentResources.markForDeletion(environmentHandle);
    }

    void Engine::destroyFrameGraph(FrameGraphHandle frameGraphHandle) {
        m_frameGraphs.markForDeletion(frameGraphHandle);
    }

    void Engine::destroyFrameResources(FrameResourcesHandle frameResourcesHandle) {
        m_frameResources.markForDeletion(frameResourcesHandle);
    }

    void Engine::destroyViewportResources(ViewportResourcesHandle viewportResourcesHandle) {
        m_viewportResources.markForDeletion(viewportResourcesHandle);
    }

    void Engine::destroySurface(SurfaceHandle surfaceHandle) {
        m_surfaces.markForDeletion(surfaceHandle);
    }

    void Engine::destroySwapchainResources(SwapchainResourcesHandle swapchainResourcesHandle) {
        m_swapchainResources.markForDeletion(swapchainResourcesHandle);
    }

    void Engine::doFrameRender(
        editor::core::MainWindow &mainWindow,
        core::Camera &camera,
        RenderIndices renderIndices,
        render::UploadSceneOutput const &scene,
        render::DeviceEnvironmentResources const &environment,
        bool isResizeMode) {

        if (m_hasFrameResources) {
            mainWindow.setFinalViewportImage(
                *m_colorAttachmentSets[renderIndices.frameIndex]);
            mainWindow.setDebugViewportImages({
                *m_debugAttachmentSets1[renderIndices.frameIndex],
                *m_debugAttachmentSets2[renderIndices.frameIndex],
                *m_debugAttachmentSets3[renderIndices.frameIndex],
                *m_debugAttachmentSets4[renderIndices.frameIndex]});
        }

        m_imGuiContext.drawUi(mainWindow);

        if (m_hasFrameResources && !isResizeMode) {
            if (!mainWindow.hasViewport())
            {
                createViewPortResources(mainWindow.getViewportExtent(), camera);
            }
            if (mainWindow.needViewPortResourcesRecreate())
            {
                recreateViewportResources(mainWindow.getViewportExtent(), camera);
            }
        }

        if (auto result = m_activeResources.swapchainManager.beginFrame(m_device, renderIndices.frameIndex);
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
            m_cameraSystems[renderIndices.frameIndex].updateData(camera);

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
                mainWindow.getMainPassSettings());

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
                .debugModeEnable = mainWindow.isDebugModeEnabled(),
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
                mainWindow.getViewportExtent());
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
            barrierCount = static_cast<uint32_t>(mainRenderPassToPresentImageBarriers.size());
        }

        cmd.pipelineBarrier2({
            .imageMemoryBarrierCount = barrierCount,
            .pImageMemoryBarriers = mainRenderPassToPresentImageBarriers.data()});

        UiPassInfo uiPassInfo{
            .colorAttachment = *m_activeResources.swapchain.imageViews[imageIndex],
            .extent = m_activeResources.swapchain.extent};

        m_imGuiContext.writeRenderCommands(cmd, uiPassInfo);

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

        auto submitRes = m_activeResources.swapchainManager.submitRenderCommands(
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

    void Engine::drawFrame(
        bool isResizeMode,
        float dt,
        pal::MainWindow &main_window,
        editor::core::MainWindow &mainWindow,
        core::Camera &camera,
        render::UploadSceneOutput const* scene,
        render::DeviceEnvironmentResources const* environment)
    {
        mainWindow.pollFileDialogs();
        mainWindow.setResizeMode(isResizeMode);

        if (scene != nullptr && environment != nullptr && !m_hasFrameResources)
        {
            updateFrameData(scene);
        }

        auto [prepareResult, renderIndices] = prepareFrame(TODO, TODO);
        if (prepareResult == vk::Result::eNotReady) {
            return;
        }
        if (prepareResult == vk::Result::eSuboptimalKHR || prepareResult == vk::Result::eErrorOutOfDateKHR) {
            recreateAllResources(TODO);
            return;
        }
        if (prepareResult != vk::Result::eSuccess) {
            throw std::runtime_error(
                "Fatal: Failed to prepare frame: " + vk::to_string(prepareResult));
        }

        doFrameRender(mainWindow, camera, renderIndices, *scene, *environment, isResizeMode);

        auto presentResult = m_activeResources.swapchainManager.present(
            m_graphicsQueue,
            *m_activeResources.swapchain.swapchain,
            renderIndices.imageIndex);

        if (presentResult == vk::Result::eSuboptimalKHR ||
            presentResult == vk::Result::eErrorOutOfDateKHR)
        {
            recreateAllResources(TODO);
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

    vk::ResultValue<vk::ShaderModule> Engine::createShaderModule(std::filesystem::path const &shaderPath) {
        std::fstream shaderModuleFile(shaderPath, std::ios::binary | std::ios::ate | std::ios::in);
        if (!shaderModuleFile.is_open())
        {
            throw std::runtime_error("Failed to open fragment shader file");
        }
        auto const shaderModuleFileSize = shaderModuleFile.tellg();
        std::vector<char> fragmentShaderCode(shaderModuleFileSize);
        shaderModuleFile.seekg(0);
        shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

        return m_device.createShaderModule(vk::ShaderModuleCreateInfo{
            .codeSize = fragmentShaderCode.size(), .pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())}, nullptr, m_dispatcher);
    }

    vk::ResultValue<vk::UniqueShaderModule> Engine::createShaderModuleUnique(std::filesystem::path const &shaderPath) const {
        std::fstream shaderModuleFile(shaderPath, std::ios::binary | std::ios::ate | std::ios::in);
        if (!shaderModuleFile.is_open())
        {
            throw std::runtime_error("Failed to open fragment shader file");
        }
        auto const shaderModuleFileSize = shaderModuleFile.tellg();
        std::vector<char> fragmentShaderCode(shaderModuleFileSize);
        shaderModuleFile.seekg(0);
        shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

        return m_device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
            .codeSize = fragmentShaderCode.size(), .pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())}, nullptr, m_dispatcher);
    }
}
