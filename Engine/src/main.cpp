#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "Sdl.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Camera/Camera.hpp"
#include "CameraController/CameraController.hpp"
#include "Render/Render.hpp"
#include "VkBootstrap.h"
#include "FrameManager/FrameManager.hpp"
#include "Painters/SunLightControlPanel/SunLightControlPanel.hpp"
#include "RetireController/RetireController.hpp"
#include "SwapchainFactory/SwapchainFactory.hpp"
#include "UiRender/UiRender.hpp"
#include "VulkanDebugger/VulkanDebugger.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace shuttle;

int main(int argc, char** argv)
{
    try
    {
        std::string sceneBlobPath = ".\\bistro";
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if ((arg == "-s" || arg == "--scene") && (i + 1) < argc)
            {
                sceneBlobPath = argv[++i];
                continue;
            }
            if (arg == "-h" || arg == "--help")
            {
                std::cout << "Usage: ShuttleEngine.exe [--scene <path-to-scene-blob>]\n";
                return 0;
            }
            if (!arg.empty() && arg[0] != '-')
            {
                sceneBlobPath = arg;
            }
        }

        const std::filesystem::path resolvedScenePath = std::filesystem::absolute(std::filesystem::path(sceneBlobPath));

        SdlLibrary sdlLibrary;
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        auto window = SdlWindow("Shuttle Engine - Adriatic Flight", 1800, 1000);

        window.setWindowCloseEventCallback(
            [&](SdlWindow&)
            {
                std::cout << "[System] Window close event received, closing window...\n";
                sdlLibrary.postQuitEvent();
            });

        auto requiredSurfaceExtensions = SdlLibrary::getSurfaceRequiredExtensions();
        requiredSurfaceExtensions.push_back(vk::EXTDebugUtilsExtensionName);
        VulkanDebugger debugger{};
        auto messengerCreateInfo = debugger.getDebugMessengerCreateInfo();

        vkb::InstanceBuilder instanceBuilder;
        auto instanceResult = instanceBuilder.set_app_name("Shuttle Engine - Adriatic Flight")
                                  .request_validation_layers(true)
                                  .set_engine_name("Shuttle Engine")
                                  .enable_extensions(requiredSurfaceExtensions)
                                    .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
                                    .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT)
                                    .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT)
                                    .set_debug_messenger_severity(messengerCreateInfo.messageSeverity)
                                  .set_debug_messenger_type(messengerCreateInfo.messageType)
                                  .set_debug_callback(messengerCreateInfo.pfnUserCallback)
                                  .require_api_version(VK_API_VERSION_1_4)
                                  .build();

        if (instanceResult.has_value())
        {
            std::cout << "[System] Vulkan instance created successfully.\n";
        }
        else
        {
            throw std::runtime_error("Failed to create Vulkan instance: " + instanceResult.error().message());
        }

        vk::Instance instance{instanceResult.value().instance};
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
        vk::UniqueInstance uniqueInstance{
            vk::Instance{instanceResult.value().instance},
            vk::UniqueHandleTraits<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                nullptr, VULKAN_HPP_DEFAULT_DISPATCHER}};

        vk::UniqueDebugUtilsMessengerEXT messenger{
            vk::DebugUtilsMessengerEXT{instanceResult.value().debug_messenger},
            vk::UniqueHandleTraits<vk::DebugUtilsMessengerEXT, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                instance, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER}};

        auto uniqueSurface = window.createVulkanSurfaceUnique(instance);

        VkPhysicalDeviceFeatures2 features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features = vk::PhysicalDeviceFeatures{.multiDrawIndirect = vk::True, .drawIndirectFirstInstance = vk::True, .fillModeNonSolid = vk::True, .samplerAnisotropy = vk::True}};
        VkPhysicalDeviceVulkan11Features features11 =
            vk::PhysicalDeviceVulkan11Features{.multiview = vk::True, .shaderDrawParameters = vk::True};
        VkPhysicalDeviceVulkan12Features features12 =
            vk::PhysicalDeviceVulkan12Features{.drawIndirectCount = vk::True,
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
            vk::PhysicalDeviceVulkan13Features{.inlineUniformBlock = vk::True,
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
            vk::PhysicalDeviceVulkan14Features{.shaderSubgroupRotate = vk::True,
                                               .shaderSubgroupRotateClustered = vk::True,
                                               .shaderExpectAssume = vk::True,
                                               .indexTypeUint8 = vk::True,
                                               .dynamicRenderingLocalRead = vk::True,
                                               .maintenance5 = vk::True,
                                               .maintenance6 = vk::True,
                                               .pipelineRobustness = vk::True,
                                               .pushDescriptor = vk::True};

        auto vkbPhysicalDeviceResult = vkb::PhysicalDeviceSelector{instanceResult.value(), *uniqueSurface}
                                           .set_minimum_version(1, 4)
                                           .add_required_extension(vk::KHRSwapchainExtensionName)
                                           .select();

        if (!vkbPhysicalDeviceResult.has_value())
        {
            throw std::runtime_error("Failed to select physical device: " + vkbPhysicalDeviceResult.error().message());
        }
        std::cout << "[System] Physical device selected: " << vkbPhysicalDeviceResult->properties.deviceName << "\n";

        auto vkbDeviceResult = vkb::DeviceBuilder{vkbPhysicalDeviceResult.value()}
                                   .set_allocation_callbacks(nullptr)
                                   .add_pNext(&features2)
                                   .add_pNext(&features11)
                                   .add_pNext(&features12)
                                   .add_pNext(&features13)
                                   .add_pNext(&features14)
                                   .build();

        if (!vkbDeviceResult.has_value())
        {
            throw std::runtime_error("Failed to create logical device: " + vkbDeviceResult.error().message());
        }
        std::cout << "[System] Vulkan instance created successfully.\n";

        auto graphicsQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::graphics);
        auto computeQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::compute);
        auto transferQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::transfer);
        auto presentationQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::present);

        vk::Queue graphicsQueue = graphicsQueueResult.value().first;

        auto graphicsQueueFamilyIndex = graphicsQueueResult.value().second;

        if (!graphicsQueueResult.has_value() || !presentationQueueResult.has_value() ||
            !computeQueueResult.has_value() || !transferQueueResult.has_value())
        {
            throw std::runtime_error("Failed to retrieve required queues from the logical device");
        }

        vk::PhysicalDevice physicalDevice = vkbPhysicalDeviceResult.value().physical_device;
        vk::Device device = vkbDeviceResult.value().device;
        VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
        vk::UniqueDevice uniqueDevice{device,
                                      vk::UniqueHandleTraits<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
                                          nullptr, VULKAN_HPP_DEFAULT_DISPATCHER}};

        // Инициализация твоего класса-обертки DeviceAllocator
        auto [createAllocatorResult, uniqueAllocator] = resources::UniqueAllocator::makeUnique(
            *uniqueInstance, *uniqueDevice, physicalDevice, vk::detail::defaultDispatchLoaderDynamic);
        if (createAllocatorResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create VMA allocator");
        }
        // Оборачиваем в наш класс ресурсов
        resources::DeviceAllocator allocator{*uniqueAllocator};

        constexpr uint32_t frameCount = 2U;

        SwapchainContext swapchainContext{.physicalDevice = physicalDevice,
                                          .device = device,
                                          .surface = *uniqueSurface,
                                          .graphicsQueueFamily = graphicsQueueFamilyIndex,
                                          .presentQueueFamily = graphicsQueueFamilyIndex};

        auto [createSwapchainResult, swapchain] = createSwapchain(swapchainContext, window.getExtent());
        if (createSwapchainResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create swapchain");
        }

        auto [createFrameManagerResult, frameManager] = FrameManager::create(device, frameCount, swapchain.imageCount);
        if (createFrameManagerResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create frameManager");
        }

        std::array descriptorPoolSizes{
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler, .descriptorCount = 16},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 64},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 512},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage,
                                   .descriptorCount = engine::render::MaxBindlessTextures + 128},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageImage, .descriptorCount = 128},
        };

        auto [createRenderDescriptorPoolResult, renderDescriptorPool] = uniqueDevice->createDescriptorPoolUnique({
            .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = 64,
            .poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
            .pPoolSizes = descriptorPoolSizes.data(),
        });
        if (createRenderDescriptorPoolResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create render descriptor pool");
        }

        engine::render::RenderContext renderContext{
            .device = device,
            .allocator = allocator,
            .swapchainColorFormat = swapchain.format,
            .swapchainDepthFormat = vk::Format::eD32Sfloat,
            .descriptorPool = std::move(renderDescriptorPool),
        };

        auto [createUniqueGraphicsCommandPoolResult, uniqueGraphicsCommandPool] =
            uniqueDevice->createCommandPoolUnique({.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                   .queueFamilyIndex = graphicsQueueFamilyIndex});
        if (createUniqueGraphicsCommandPoolResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create graphics command pool");
        }

        auto [createRendererResourcesResult, rendererResources] =
            engine::render::createRendererResources(renderContext, graphicsQueue, *uniqueGraphicsCommandPool);
        if (createRendererResourcesResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create renderer resources");
        }

        std::cout << "[Scene] Loading pre-compiled .scene blob: " << resolvedScenePath.string() << "\n";
        auto [uploadSceneResult, uploadSceneOutput] = engine::render::uploadScene(
            "BistroExterior.sblb", renderContext, graphicsQueue, *uniqueGraphicsCommandPool, *rendererResources.sceneSetLayout,
            *rendererResources.fallbackAlbedoImageView, *rendererResources.fallbackNormalImageView,
            *rendererResources.fallbackOrmImageView, *rendererResources.fallbackEmissionImageView);
        if (uploadSceneResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to upload scene resources");
        }

        auto [createEnvironmentResourcesResult, environmentResources] = engine::render::createEnvironmentResources(
            renderContext, rendererResources, graphicsQueue, *uniqueGraphicsCommandPool, "Skybox.sblb");
        if (createEnvironmentResourcesResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create environment resources");
        }

        auto [createRenderTargetsResult, renderTargets] =
            engine::render::createRenderTargets(device, allocator, swapchain.images, swapchain.extent, swapchain.format);
        if (createRenderTargetsResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create render targets");
        }

        std::vector<engine::render::DeviceFrameResources> frameResources;
        frameResources.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            auto [createFrameResourcesResult, frameResource] = engine::render::createFrameResources(
                renderContext, rendererResources, swapchain.extent.width, swapchain.extent.height,
                uploadSceneOutput.sceneFrameRequirements.drawableObjectCount,
                uploadSceneOutput.sceneFrameRequirements.transformCount,
                uploadSceneOutput.sceneFrameRequirements.meshCount,
                engine::render::ShadowSettings{}.resolution,
                engine::render::ShadowSettings{}.cascadeCount);
            if (createFrameResourcesResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Failed to create frame resources");
            }
            frameResources.push_back(std::move(frameResource));
        }

        std::vector<resources::UniqueAllocatedBuffer> frustumUploadBuffers;
        frustumUploadBuffers.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            auto [createUploadBufferResult, uploadBuffer] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{.size = sizeof(engine::render::FrustumPlanesData) * engine::render::MaxFrustums,
                                     .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                     .sharingMode = vk::SharingMode::eExclusive},
                resources::MemoryUsage::eCpuOnly,
                resources::AllocationCreateFlagBits::eMapped);
            if (createUploadBufferResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("Failed to create frustum upload buffer");
            }
            frustumUploadBuffers.push_back(std::move(uploadBuffer));
        }

        Camera camera{glm::vec3{10.0f, 30.3f, 0.0f}};
        camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        camera.setWindowSize(swapchain.extent.width, swapchain.extent.height);

        CameraController cameraController{camera};
        window.setKeyboardEventCallback(
            [&](SdlWindow&, SdlKeyCode keyCode, SdlKeyMode, SdlKeyState keyState)
            {
                cameraController.handleKeyboardEvent(window, keyCode, SdlKeyModeBits::None, keyState, sdlLibrary);
            });

        uint32_t currentFrameIndex = 0U;

        auto uiRenderResultValue = UiRender::create(window, instance, physicalDevice, device, graphicsQueueFamilyIndex,
                                                    graphicsQueue, swapchain.imageCount);

        auto uiRender = std::move(uiRenderResultValue.value);
        uiRender.bindInputEventHandler(sdlLibrary);

        auto lastTime = std::chrono::high_resolution_clock::now();
        float elapsedTime = 0.0f;
        glm::mat4 previousViewProjectionMatrix = camera.getProjectionMatrix() * camera.getViewMatrix();

        glm::vec4 sunDirection{0.0f, -1.0f, 0.0f, 0.0f};
        glm::vec4 sunColor{1.0f, 1.0f, 1.0f, 1.0f};
        float sunIntensity = 1.0f;
        if (!uploadSceneOutput.hostSceneData.directionalLights.empty())
        {
            auto const& sun = uploadSceneOutput.hostSceneData.directionalLights[0];
            sunDirection = sun.directionAndIntensity;
            sunColor = glm::vec4{sun.color, 1.0f};
            sunIntensity = sun.directionAndIntensity.w;
        }
        auto [allocateGraphicsCommandBufferResult, uniqueGraphicsCommandBuffers] =
            uniqueDevice->allocateCommandBuffersUnique({.commandPool = *uniqueGraphicsCommandPool,
                                                        .level = vk::CommandBufferLevel::ePrimary,
                                                        .commandBufferCount = frameCount});
        if (allocateGraphicsCommandBufferResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create graphics command buffer");
        }

        bool isMinimized = false;
        window.setWindowShowModeEventCallback(
            [&isMinimized](SdlWindow const& window, ShowMode showMode)
            {
                if (showMode == ShowMode::Minimized)
                {
                    isMinimized = true;
                }
                else
                    isMinimized = false;
            });

        SwapchainResources activeResources{.swapchain = std::move(swapchain),
                                           .frameManager = std::move(frameManager),
                                           .renderTargets = std::move(renderTargets)};
        std::vector swapchainImageLayouts(activeResources.swapchain.imageCount, vk::ImageLayout::eUndefined);

        RetireController retireController{};

        auto recreateAllResources = [&]
        {
            auto [result, newResources] = retireController.updateSwapchainResources(
                swapchainContext, window.getExtent(), allocator, frameCount, std::move(activeResources));

            if (result != vk::Result::eSuccess)
            {
                throw std::runtime_error("Fatal: Swapchain recreation failed");
            }

            activeResources = std::move(newResources);
            swapchainImageLayouts.assign(activeResources.swapchain.imageCount, vk::ImageLayout::eUndefined);



            camera.setWindowSize(activeResources.swapchain.extent.width, activeResources.swapchain.extent.height);
        };

        auto extractFrustumPlanes = [](glm::mat4 const& viewProjection)
        {
            auto normalizePlane = [](glm::vec4 plane)
            {
                const float length = glm::length(glm::vec3{plane.x, plane.y, plane.z});
                if (length > 0.000001f)
                {
                    plane /= length;
                }
                return plane;
            };

            std::array<glm::vec4, 6> planes{};
            const glm::vec4 row0{viewProjection[0][0], viewProjection[1][0], viewProjection[2][0], viewProjection[3][0]};
            const glm::vec4 row1{viewProjection[0][1], viewProjection[1][1], viewProjection[2][1], viewProjection[3][1]};
            const glm::vec4 row2{viewProjection[0][2], viewProjection[1][2], viewProjection[2][2], viewProjection[3][2]};
            const glm::vec4 row3{viewProjection[0][3], viewProjection[1][3], viewProjection[2][3], viewProjection[3][3]};

            planes[0] = normalizePlane(row3 + row0);
            planes[1] = normalizePlane(row3 - row0);
            planes[2] = normalizePlane(row3 + row1);
            planes[3] = normalizePlane(row3 - row1);
            planes[4] = normalizePlane(row2);
            planes[5] = normalizePlane(row3 - row2);
            return planes;
        };

        std::cout << "[Run] Entering main render loop. Engine is green.\n";

        while (true)
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;
            elapsedTime += deltaTime;

            if (!sdlLibrary.pullEvents()) break;
            if (isMinimized) continue;
            cameraController.update(deltaTime);

            auto prepareRes = activeResources.frameManager.prepareFrameSlot(device, currentFrameIndex);
            if (prepareRes != vk::Result::eSuccess)
            {
                throw std::runtime_error("Fatal: Failed to prepare frame slot");
            }

            uiRender.drawUi(SunLightControlPanel{sunDirection, sunColor, sunIntensity});

            retireController.renderRetireUpdate(currentFrameIndex);

            auto acquireResult = activeResources.frameManager.acquireNextImage(
                device, *activeResources.swapchain.swapchain, currentFrameIndex);

            if (acquireResult.result == vk::Result::eNotReady)
            {
                continue;
            }

            if (acquireResult.result == vk::Result::eSuccess)
            {
                uint32_t const imageIndex = acquireResult.value;
                retireController.presentRetireUpdate(imageIndex);

                if (!uploadSceneOutput.hostSceneData.directionalLights.empty())
                {
                    auto& sceneSun = uploadSceneOutput.hostSceneData.directionalLights[0];
                    sceneSun.directionAndIntensity = glm::vec4{sunDirection.x, sunDirection.y, sunDirection.z, sunIntensity};
                    sceneSun.color = glm::vec3{sunColor.x, sunColor.y, sunColor.z};
                }

                auto& frameResource = frameResources[currentFrameIndex];
                auto* frameInfo = static_cast<engine::render::FrameInfo*>(
                    allocator.getMappedPointer(*frameResource.frameInfoBuffer));
                if (frameInfo == nullptr)
                {
                    throw std::runtime_error("Failed to map frame info buffer");
                }

                const glm::mat4 view = camera.getViewMatrix();
                const glm::mat4 projection = camera.getProjectionMatrix();
                const glm::mat4 viewProjection = projection * view;

                frameInfo->viewMatrix = view;
                frameInfo->projectionMatrix = projection;
                frameInfo->viewProjectionMatrix = viewProjection;
                frameInfo->previousViewProjectionMatrix = previousViewProjectionMatrix;
                frameInfo->cameraPosition = glm::vec4{camera.getPosition(), 1.0f};
                frameInfo->renderResolution =
                    glm::vec2{static_cast<float>(activeResources.swapchain.extent.width),
                              static_cast<float>(activeResources.swapchain.extent.height)};
                frameInfo->invRenderResolution = glm::vec2{
                    1.0f / std::max(frameInfo->renderResolution.x, 1.0f),
                    1.0f / std::max(frameInfo->renderResolution.y, 1.0f),
                };
                frameInfo->displayResolution = frameInfo->renderResolution;
                frameInfo->invDisplayResolution = frameInfo->invRenderResolution;
                frameInfo->deltaTime = deltaTime;
                frameInfo->elapsedTime = elapsedTime;
                frameInfo->frameIndex = currentFrameIndex;
                frameInfo->drawableCount = uploadSceneOutput.sceneFrameRequirements.drawableObjectCount;
                frameInfo->nearPlane = 0.1f;
                frameInfo->farPlane = 1000.0f;
                frameInfo->exposure = 1.0f;
                frameInfo->gamma = 2.2f;
                frameInfo->frustumCount = 1;
                frameInfo->shadowCascadeCount = engine::render::ShadowSettings{}.cascadeCount;

                engine::render::FrustumPlanesData frustumPlanes{};
                const auto cameraFrustum = extractFrustumPlanes(viewProjection);
                for (size_t i = 0; i < cameraFrustum.size(); ++i)
                {
                    frustumPlanes.planes[i] = cameraFrustum[i];
                }

                auto* frustumUploadData = static_cast<engine::render::FrustumPlanesData*>(
                    allocator.getMappedPointer(*frustumUploadBuffers[currentFrameIndex]));
                if (frustumUploadData == nullptr)
                {
                    throw std::runtime_error("Failed to map frustum upload buffer");
                }
                std::memcpy(frustumUploadData, &frustumPlanes, sizeof(frustumPlanes));

                engine::render::CascadeSetupPushConstants cascadePushConstants{};
                cascadePushConstants.lightDirection = sunDirection;
                cascadePushConstants.shadowDistance = engine::render::ShadowSettings{}.maxDistance;
                cascadePushConstants.splitLambda = engine::render::ShadowSettings{}.splitLambda;
                cascadePushConstants.cascadeCount = engine::render::ShadowSettings{}.cascadeCount;
                cascadePushConstants.shadowMapResolution = engine::render::ShadowSettings{}.resolution;
                cascadePushConstants.depthPadding = 100.0f;

                vk::CommandBuffer cmd = uniqueGraphicsCommandBuffers[currentFrameIndex].get();
                if (auto resetResult = cmd.reset(); resetResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Failed to reset graphics command buffer");
                }
                if (auto beginResult = cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
                    beginResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Failed to begin graphics command buffer");
                }


                cmd.copyBuffer(
                    *frustumUploadBuffers[currentFrameIndex],
                    *frameResource.frustumPlanesBuffer,
                    vk::BufferCopy{.srcOffset = 0, .dstOffset = 0, .size = sizeof(engine::render::FrustumPlanesData)});

                vk::BufferMemoryBarrier2 frustumBarrier{
                    .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                    .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                    .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .buffer = *frameResource.frustumPlanesBuffer,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE,
                };
                cmd.pipelineBarrier2(vk::DependencyInfo{.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &frustumBarrier});



                if (auto endResult = cmd.end(); endResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Failed to end graphics command buffer");
                }

                auto submitRes = activeResources.frameManager.submitRenderCommands(graphicsQueue, cmd,
                                                                                   currentFrameIndex, imageIndex);
                if (submitRes != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Failed to submit command buffer to graphics queue");
                }

                auto presentResult = activeResources.frameManager.present(
                    graphicsQueue, *activeResources.swapchain.swapchain, imageIndex);

                if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR)
                {
                    recreateAllResources();
                }
                else if (presentResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error("Fatal: Failed to present rendered image to queue");
                }
                else
                {
                    swapchainImageLayouts[imageIndex] = vk::ImageLayout::ePresentSrcKHR;
                }

                previousViewProjectionMatrix = viewProjection;
                currentFrameIndex = (currentFrameIndex + 1) % frameCount;
            }
            else if (acquireResult.result == vk::Result::eSuboptimalKHR ||
                     acquireResult.result == vk::Result::eErrorOutOfDateKHR)
            {
                recreateAllResources();
            }
            else
            {
                throw std::runtime_error("Fatal: Failed to acquire next image from swapchain!");
            }
        }

        if (uniqueDevice->waitIdle() != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to wait for device idle during shutdown");
        }
        uiRender.destroy(device);
        std::cout << "[Shutdown] Device idle confirmed. Exiting cleanly.\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[CRITICAL ERROR] " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}