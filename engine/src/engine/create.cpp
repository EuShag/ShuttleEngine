#include "engine_impl.hpp"
#include "VkBootstrap.h"
#include "vulkan_debugger/vulkan_debugger.hpp"

namespace shuttle::engine {
    vk::ResultValue<EngineImpl> EngineImpl::create() {
        vk::detail::DynamicLoader dynamicLoader;
        vk::detail::DispatchLoaderDynamic dispatchLoaderDynamic;

        dispatchLoaderDynamic.init(dynamicLoader);

        // Получаем расширения поверхности от платформенного backend'а.
        auto requiredSurfaceExtensions = pal::Platform::getSurfaceRequiredExtensions();
        requiredSurfaceExtensions.push_back(vk::EXTDebugUtilsExtensionName);

        char const* layerNames[] = {"VK_LAYER_KHRONOS_validation"};

        VulkanDebugger debugger{};
        auto messengerCreateInfo = debugger.getDebugMessengerCreateInfoHpp();

        vk::ApplicationInfo applicationInfo{
            .pApplicationName = "Shuttle Editor",
            .applicationVersion = vk::makeApiVersion(0, 0, 3, 0),
            .pEngineName = "Shuttle Engine",
            .engineVersion = vk::makeApiVersion(0, 0, 3, 0),
            .apiVersion = vk::makeApiVersion(0, 1, 4, 0)
        };

        vk::StructureChain instanceCreateStructureChain {
            vk::InstanceCreateInfo {
                .pApplicationInfo = &applicationInfo,
                .enabledLayerCount = 1,
                .ppEnabledLayerNames = layerNames,
                .enabledExtensionCount = static_cast<uint32_t>(requiredSurfaceExtensions.size()),
                .ppEnabledExtensionNames = requiredSurfaceExtensions.data()
            },
            debugger.getDebugMessengerCreateInfoHpp()
        };

        auto [createInstanceResult, instance] = vk::createInstanceUnique(instanceCreateStructureChain.get(), nullptr, dispatchLoaderDynamic);
        if (createInstanceResult != vk::Result::eSuccess) return {createInstanceResult, {}};
        dispatchLoaderDynamic.init(*instance);

        auto [enumeratePhysicalDevicesResult, physicalDevices] = instance->enumeratePhysicalDevices();
        if (enumeratePhysicalDevicesResult != vk::Result::eSuccess) return {enumeratePhysicalDevicesResult, {}};

        vk::PhysicalDevice physicalDevice = VK_NULL_HANDLE;
        for (auto physicalDevice_ : physicalDevices) {
            auto properties = physicalDevice_.getProperties(dispatchLoaderDynamic);
            if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                physicalDevice = physicalDevice_;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) physicalDevice = physicalDevices[0];

        float queuePriority = 1.0f;

        vk::DeviceQueueCreateInfo deviceQueueCreateInfo = {
            .queueFamilyIndex = 0,
            .queueCount = 0,
            .pQueuePriorities = &queuePriority
        };

        char const* enabledDeviceExtensions[] = {vk::KHRSwapchainExtensionName, vk::KHRSwapchainMaintenance1ExtensionName};

        vk::StructureChain deviceCreateInfoStructureChain = {
            vk::DeviceCreateInfo {
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &deviceQueueCreateInfo,
                .enabledExtensionCount = 2,
                .ppEnabledExtensionNames = enabledDeviceExtensions
            },

            vk::PhysicalDeviceFeatures2{
                .features = vk::PhysicalDeviceFeatures{
                    .imageCubeArray = vk::True,
                    .multiDrawIndirect = vk::True,
                    .drawIndirectFirstInstance = vk::True,
                    .fillModeNonSolid = vk::True,
                    .samplerAnisotropy = vk::True,
                    .shaderInt64 = vk::True
                }},

            vk::PhysicalDeviceVulkan11Features{
                .multiview = vk::True,
                .shaderDrawParameters = vk::True},

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
                .shaderOutputLayer = vk::True},

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
                .maintenance4 = vk::True},

            vk::PhysicalDeviceVulkan14Features{
                .shaderSubgroupRotate = vk::True,
                .shaderSubgroupRotateClustered = vk::True,
                .shaderExpectAssume = vk::True,
                .indexTypeUint8 = vk::True,
                .dynamicRenderingLocalRead = vk::True,
                .maintenance5 = vk::True,
                .maintenance6 = vk::True,
                .pipelineRobustness = vk::True,
                .pushDescriptor = vk::True},

            vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR{
                .swapchainMaintenance1 = vk::True}
        };

        auto [createDeviceResult, device] = physicalDevice.createDeviceUnique(
            deviceCreateInfoStructureChain.get(),
            nullptr,
            dispatchLoaderDynamic
        );
        if (createDeviceResult != vk::Result::eSuccess) return {createDeviceResult, {}};

        dispatchLoaderDynamic.init(*device);

        auto [createAllocatorResult, uniqueAllocator] =
            resources::UniqueAllocator::makeUnique(
                *instance,
                *device,
                physicalDevice,
                dispatchLoaderDynamic);

        if (createAllocatorResult != vk::Result::eSuccess) return {createAllocatorResult, {}};

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
}
