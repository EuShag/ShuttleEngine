#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <fstream>
#include <iostream>
#include <chrono>
#include <utility>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "EnvironmentBlobLoader/EnvironmentBlobLoader.hpp"
#include "Sdl.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Camera/Camera.hpp"
#include "CameraController/CameraController.hpp" // Наш новый класс контроля
#include "PbrRender/Render.hpp"       // Твой рендерер с тенями
#include "BlobLoader/BlobLoader.hpp"           // Pre-compiled scene loader
#include "VkBootstrap.h"
#include "FrameManager/FrameManager.hpp"
#include "Painters/SunLightControlPanel/SunLightControlPanel.hpp"
#include "RetireController/RetireController.hpp"
#include "SwapchainFactory/SwapchainFactory.hpp"
#include "Terrain/Terrain.hpp"
#include "UiRender/UiRender.hpp"
#include "VulkanDebugger/VulkanDebugger.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace shuttle_engine;

int main(int argc, char** argv) {
	try {
		std::string sceneBlobPath = ".\\lowe.shscene";
		for (int i = 1; i < argc; ++i) {
			const std::string arg = argv[i];
			if ((arg == "-s" || arg == "--scene") && (i + 1) < argc) {
				sceneBlobPath = argv[++i];
				continue;
			}
			if (arg == "-h" || arg == "--help") {
				std::cout << "Usage: ShuttleEngine.exe [--scene <path-to-scene-blob>]\n";
				return 0;
			}
			if (!arg.empty() && arg[0] != '-') {
				sceneBlobPath = arg;
			}
		}

		const std::filesystem::path resolvedScenePath = std::filesystem::absolute(std::filesystem::path(sceneBlobPath));

		// =========================================================================
		// 1. ИНИЦИАЛИЗАЦИЯ ОКНА И СИСТЕМЫ СИГНАЛОВ
		// =========================================================================
		SdlLibrary sdlLibrary; // Инициализирует SDL
		VULKAN_HPP_DEFAULT_DISPATCHER.init();

		auto window = SdlWindow("Shuttle Engine - Adriatic Flight", 1800, 1000);

		window.setWindowCloseEventCallback([&] (SdlWindow&) {
			std::cout << "[System] Window close event received, closing window...\n";
			sdlLibrary.postQuitEvent();
		});

		auto requiredSurfaceExtensions = SdlLibrary::getSurfaceRequiredExtensions();
		requiredSurfaceExtensions.push_back(vk::EXTDebugUtilsExtensionName);
		VulkanDebugger debugger{};
		auto messengerCreateInfo = debugger.getDebugMessengerCreateInfo();

		// =========================================================================
		// 2. СОЗДАНИЕ VULKAN INSTANCE, SURFACE И PHYSICAL DEVICE (Твой родной код)
		// =========================================================================
		vkb::InstanceBuilder instanceBuilder;
		auto instanceResult = instanceBuilder.
			set_app_name("Shuttle Engine - Adriatic Flight").
			request_validation_layers(true).
			set_engine_name("Shuttle Engine").
			enable_extensions(requiredSurfaceExtensions).
			set_debug_messenger_severity(messengerCreateInfo.messageSeverity).
			set_debug_messenger_type(messengerCreateInfo.messageType).
			set_debug_callback(messengerCreateInfo.pfnUserCallback).
			require_api_version(VK_API_VERSION_1_4).
			build();

		if (instanceResult.has_value()) {
			std::cout << "[System] Vulkan instance created successfully.\n";
		} else {
			throw std::runtime_error("Failed to create Vulkan instance: " + instanceResult.error().message());
		}

		vk::Instance instance{instanceResult.value().instance};
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
		vk::UniqueInstance uniqueInstance{
			vk::Instance{instanceResult.value().instance},
			vk::UniqueHandleTraits<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
				nullptr,
				VULKAN_HPP_DEFAULT_DISPATCHER}
		};

		vk::UniqueDebugUtilsMessengerEXT messenger{
			vk::DebugUtilsMessengerEXT{instanceResult.value().debug_messenger},
			vk::UniqueHandleTraits<vk::DebugUtilsMessengerEXT, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{
				instance,
				nullptr,
				VULKAN_HPP_DEFAULT_DISPATCHER
			}
		};

		auto uniqueSurface = window.createVulkanSurfaceUnique(instance);

		VkPhysicalDeviceFeatures2 features2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.features = vk::PhysicalDeviceFeatures{
				.multiDrawIndirect = vk::True,
				.samplerAnisotropy = vk::True
			}
		};
		VkPhysicalDeviceVulkan11Features features11 = vk::PhysicalDeviceVulkan11Features{
			.multiview = vk::True,
			.shaderDrawParameters = vk::True
		};
		VkPhysicalDeviceVulkan12Features features12 = vk::PhysicalDeviceVulkan12Features{
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
			.shaderOutputLayer = vk::True
		};
		VkPhysicalDeviceVulkan13Features features13 = vk::PhysicalDeviceVulkan13Features{
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
			.maintenance4 = vk::True
		};
		VkPhysicalDeviceVulkan14Features features14 = vk::PhysicalDeviceVulkan14Features{
			.shaderSubgroupRotate = vk::True,
			.shaderSubgroupRotateClustered = vk::True,
			.shaderExpectAssume = vk::True,
			.indexTypeUint8 = vk::True,
			.dynamicRenderingLocalRead = vk::True,
			.maintenance5 = vk::True,
			.maintenance6 = vk::True,
			.pipelineRobustness = vk::True,
			.pushDescriptor = vk::True
		};

		auto vkbPhysicalDeviceResult = vkb::PhysicalDeviceSelector{instanceResult.value(), *uniqueSurface}.
			set_minimum_version(1, 4).
			add_required_extension(vk::KHRSwapchainExtensionName).
		select();

		if (!vkbPhysicalDeviceResult.has_value()) {
			throw std::runtime_error("Failed to select physical device: " + vkbPhysicalDeviceResult.error().message());
		}
		std::cout << "[System] Physical device selected: " << vkbPhysicalDeviceResult->properties.deviceName << "\n";

		auto vkbDeviceResult = vkb::DeviceBuilder{vkbPhysicalDeviceResult.value()}.
			set_allocation_callbacks(nullptr).
			add_pNext(&features2).
			add_pNext(&features11).
			add_pNext(&features12).
			add_pNext(&features13).
			add_pNext(&features14).
			build();

		if (!vkbDeviceResult.has_value()) {
			throw std::runtime_error("Failed to create logical device: " + vkbDeviceResult.error().message());
		}
		std::cout << "[System] Vulkan instance created successfully.\n";

		auto graphicsQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::graphics);
		auto computeQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::compute);
		auto transferQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::transfer);
		auto presentationQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::present);

		vk::Queue graphicsQueue = graphicsQueueResult.value().first;
		vk::Queue presentationQueue = presentationQueueResult.value().first;
		vk::Queue transferQueue = transferQueueResult.value().first;

		auto graphicsQueueFamilyIndex = graphicsQueueResult.value().second;
		auto transferQueueFamilyIndex = transferQueueResult.value().second;
		auto presentationQueueFamilyIndex = presentationQueueResult.value().second;

		if (!graphicsQueueResult.has_value() || !presentationQueueResult.has_value() || !computeQueueResult.has_value() || !transferQueueResult.has_value()) {
			throw std::runtime_error("Failed to retrieve required queues from the logical device");
		}

		vk::PhysicalDevice physicalDevice = vkbPhysicalDeviceResult.value().physical_device;
		vk::Device device = vkbDeviceResult.value().device;
		VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
		vk::UniqueDevice uniqueDevice{
			device,
			vk::UniqueHandleTraits<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{ nullptr, VULKAN_HPP_DEFAULT_DISPATCHER }
		};

		// Инициализация твоего класса-обертки DeviceAllocator
		auto [createAllocatorResult, uniqueAllocator] = resources::UniqueAllocator::makeUnique(
			*uniqueInstance,
			*uniqueDevice,
			physicalDevice,
			vk::detail::defaultDispatchLoaderDynamic
		);
		if (createAllocatorResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create VMA allocator");
		}
		// Оборачиваем в наш класс ресурсов
		resources::DeviceAllocator allocator{ *uniqueAllocator };

		// Создаем Command Pool для трансфера ресурсов на GPU
		auto [createCommandPoolResult, uniqueTransferCommandPool] = uniqueDevice->createCommandPoolUnique(
			vk::CommandPoolCreateInfo{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = transferQueueFamilyIndex
			}
		);
		if (createCommandPoolResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create transfer command pool");
		}

		// =========================================================================
		// 5. ИНИЦИАЛИЗАЦИЯ ИНДУСТРИАЛЬНОГО РЕНДЕРЕРА SHUTTLE ENGINE
		// =========================================================================
		std::cout << "[Init] Initializing PbrRender pipeline...\n";
		auto [pbrRes, pbrRender] = PbrRender::create(
			*uniqueDevice, vk::ImageLayout::ePresentSrcKHR,
			transferQueue,
			*uniqueTransferCommandPool,
			allocator
		);
		if (pbrRes != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create PbrRender system");
		}

		// =========================================================================
		// 6. LOAD PRE-COMPILED SCENE FROM BLOB FILE
		// =========================================================================
		std::cout << "[Scene] Loading pre-compiled .scene blob: " << resolvedScenePath.string() << "\n";
		BlobSceneData globalScene = BlobLoader::load(resolvedScenePath.string());
		auto environment = assets::BlobEnvironmentData::loadFromFile("studio.envb");

		std::cout << "[Scene] Blob stats: textures=" << globalScene.textures.size()
			<< " materials=" << globalScene.materials.size()
			<< " meshes=" << globalScene.meshes.size() << "\n";

		// Terrain mesh is still generated from a heightmap (separate from blob path).
		TerrainProperties terrainProperties{
			.meshResolution = vk::Extent2D{
				.width = 2096,
				.height = 2096,
			},
			.worldSize = {512.0f, 512.0f},
			.minHeight = -40.0f,
			.maxHeight = 40.0f
		};
		// NOTE: terrain material loading (loadFromFiles) removed along with the old Image loader.
		// Terrain geometry generation still works via Image1D16bit + TerrainGeometryGenerator.
		// To re-enable terrain rendering, upload the HostMeshData separately and add it to the
		// scene blob via AssetProcessor.

		std::cout << "Skybox size: " << environment.skybox().data.size() << std::endl;
		std::cout << "Irradiance size: " << environment.irradiance().data.size() << std::endl;
		std::cout << "Radiance size: " << environment.radiance().data.size() << std::endl;

		// Upload everything to GPU
		std::cout << "[Scene] Uploading buffers and textures to GPU Local memory...\n";
		auto [uploadRes, deviceSceneData] = pbrRender.uploadScene(
			globalScene,
			&environment,
			transferQueue,
			*uniqueDevice,
			*uniqueTransferCommandPool,
			allocator
		);
		if (uploadRes != vk::Result::eSuccess) {
			throw std::runtime_error("CRITICAL: Failed to upload scene data to GPU");
		}
		std::cout << "[Scene] Scene uploaded successfully. Render-ready.\n";

		// =========================================================================
		// 7. СОЗДАНИЕ СИСТЕМЫ ПЛАВНОГО УПРАВЛЕНИЯ КАМЕРОЙ (NEW!)f
		// =========================================================================
		Camera camera{ glm::vec3{10.0f, 30.3f, 0.0f} };
		camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));

		CameraController cameraController{ camera };

		bool needsMadeScreenshoot = false;
		// Регистрируем управление через нашу обертку
		window.setKeyboardEventCallback([&](SdlWindow&, SdlKeyCode keyCode, SdlKeyMode, SdlKeyState keyState) {
			cameraController.handleKeyboardEvent(window, keyCode, SdlKeyModeBits::None, keyState, sdlLibrary);
			if (keyState == SdlKeyState::Pressed && keyCode == SdlKeyCode::F12) needsMadeScreenshoot = true;
		});

		// =========================================================================
		// 9. ГЛАВНЫЙ ЦИКЛ ОТРЫВА ОТ ЗЕМЛИ (DYNAMICAL GAME LOOP)
		// =========================================================================

		std::array descriptorPoolSizes {
			vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 10
			},
			vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 10
			},
			vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eSampledImage,
				.descriptorCount = 10
			}
		};

		auto [createFrameDataDescriptorPool, frameDataDescriptorPool] = device.createDescriptorPoolUnique(
			{
				.maxSets = 10,
				.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
				.pPoolSizes = descriptorPoolSizes.data()
			}
		);

		SwapchainContext swapchainContext{
			.physicalDevice = physicalDevice,
			.device = device,
			.surface = *uniqueSurface,
			.graphicsQueueFamily = graphicsQueueFamilyIndex,
			.presentQueueFamily = presentationQueueFamilyIndex
		};

		auto [createSwapchainResult, swapchain] = createSwapchain(
			swapchainContext,
			window.getExtent()
		);
		if (createSwapchainResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create swapchain");
		}

		auto [createRenderTargetsResult, renderTargets] = pbrRender.createRenderTargets(
			device,
			allocator,
			swapchain.images,
			swapchain.extent
		);

		if (createRenderTargetsResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create render targets");
		}

		uint32_t frameCount = 2U;
		uint32_t currentFrameIndex = 0U;

		auto [createFrameManagerResult, frameManager] = FrameManager::create(device, frameCount, swapchain.imageCount);
		if (createFrameManagerResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create frameManager");
		}

		auto [createFrameDatasResult, frameDatas] = pbrRender.createFrameDatas(
			device,
			allocator,
			{ .width = 4096, .height = 4096 },
			swapchain.extent,
			*frameDataDescriptorPool,
			frameCount
		);

		if (createFrameDatasResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create frame datas");
		}

		auto uiRenderResultValue = UiRender::create(
			window,
			instance,
			physicalDevice,
			device,
			graphicsQueueFamilyIndex,
			graphicsQueue,
			swapchain.imageCount
		);

		auto uiRender = std::move(uiRenderResultValue.value);
		uiRender.bindInputEventHandler(sdlLibrary);

		// Запуск таймера С++20 для расчета Delta Time
		auto lastTime = std::chrono::high_resolution_clock::now();

		std::cout << "[Run] Entering main render loop. Engine is green.\n";

		SunLightControlPanel sunLightControlPanel{
			deviceSceneData.directionalLightDatas[0].direction,
			deviceSceneData.directionalLightDatas[0].color,
			deviceSceneData.directionalLightDatas[0].color.a,
		};

		auto [createUniqueGraphicsCommandPoolResult, uniqueGraphicsCommandPool] = uniqueDevice->createCommandPoolUnique(
			{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = graphicsQueueFamilyIndex
			}
		);
		if (createUniqueGraphicsCommandPoolResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create graphics command pool");
		}
		auto [allocateGraphicsCommandBufferResult, uniqueGraphicsCommandBuffers] = uniqueDevice->allocateCommandBuffersUnique(
			{
				.commandPool = *uniqueGraphicsCommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = frameCount
			}
		);
		if (allocateGraphicsCommandBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create graphics command buffer");
		}

		bool isMinimized = false;
		window.setWindowShowModeEventCallback( [&isMinimized](SdlWindow const& window, ShowMode showMode) {
			if (showMode == ShowMode::Minimized) {
				isMinimized = true;
			}
			else isMinimized = false;
		});

		SwapchainResources activeResources{
			.swapchain = std::move(swapchain),
			.frameManager = std::move(frameManager),
			.renderTargets = std::move(renderTargets)
		};
		RetireController retireController{};

		// Локальный хелпер для безопасного и плавного пересоздания ресурсов
		auto recreateAllResources = [&] {

			auto [result, newResources] = retireController.updateSwapchainResources(
				swapchainContext,
				window.getExtent(),
				allocator,
				pbrRender,
				frameCount,
				std::move(activeResources) // Передаем старые ресурсы в RetireController
			);

			if (result != vk::Result::eSuccess) {
				throw std::runtime_error("Fatal: Swapchain recreation failed");
			}

			// Активируем новые ресурсы
			activeResources = std::move(newResources);

			// Обновляем матрицу проекции камеры под новое соотношение сторон
			camera.setWindowSize(activeResources.swapchain.extent.width, activeResources.swapchain.extent.height);
		};

		while (true) {
			// ВычисляемdeltaTime
			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
			lastTime = currentTime;

			// -----------------------------------------------------------------
			// ФАЗА 1: Опрос и фильтрация событий ОС (До всякой синхронизации GPU!)
			// -----------------------------------------------------------------
			if (!sdlLibrary.pullEvents()) break;
			if (isMinimized) continue;
			cameraController.update(deltaTime);

			// -----------------------------------------------------------------
			// ФАЗА 2: Синхронизация CPU-GPU (Ожидание фенса текущего слота кадра)
			// -----------------------------------------------------------------
			auto prepareRes = activeResources.frameManager.prepareFrameSlot(device, currentFrameIndex);
			if (prepareRes != vk::Result::eSuccess) {
				throw std::runtime_error("Fatal: Failed to prepare frame slot");
			}

			uiRender.drawUi(std::move(sunLightControlPanel));

			// CPU-часть синхронизации завершена!
			// Этот слот кадра гарантированно освободился на GPU -> поджигаем бит в renderMask у пенсионеров
			retireController.renderRetireUpdate(currentFrameIndex);

			// -----------------------------------------------------------------
			// ФАЗА 3: Запрос свободного изображения из свопчейна (Acquire)
			// -----------------------------------------------------------------
			auto acquireResult = activeResources.frameManager.acquireNextImage(
				device,
				*activeResources.swapchain.swapchain,
				currentFrameIndex
			);

			if (acquireResult.result == vk::Result::eSuccess) {
				uint32_t const imageIndex = acquireResult.value;

				// Изображение успешно получено -> поджигаем бит в presentMask у пенсионеров.
				// Если все биты рендера и презентации совпали с целевыми, старые ресурсы тихо умрут прямо сейчас.
				retireController.presentRetireUpdate(imageIndex);

				// -------------------------------------------------------------
				// ФАЗА 4: Обновление игровой логики и данных на GPU
				// -------------------------------------------------------------

				// Загружаем обновленные матрицы камеры и свет в UBO/SSBO на GPU
				auto updateRes = PbrRender::updateSceneData(
					allocator,
					deviceSceneData,
					frameDatas[currentFrameIndex],
					camera.getViewMatrix(),
					camera.getProjectionMatrix(),
					camera.getShortProjectionMatrix(),
					camera.getPosition()
				);
				if (updateRes != vk::Result::eSuccess) {
					throw std::runtime_error("Failed to update scene data buffer on GPU");
				}

				// -------------------------------------------------------------
				// ФАЗА 6: Запись Vulkan команд (Command Recording)
				// -------------------------------------------------------------
				vk::CommandBuffer cmd = uniqueGraphicsCommandBuffers[currentFrameIndex].get();
				cmd.reset();
				cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

				// Записываем проход рендеринга сцены
				pbrRender.recordRenderFrameCommands(
					deviceSceneData,
					cmd,
					frameDatas[currentFrameIndex],
					activeResources.renderTargets[imageIndex], // Рендерим в таргет текущей картинки
					[&](vk::CommandBuffer drawCmd) {
						uiRender.recordDrawCommands(drawCmd);
					},
					needsMadeScreenshoot
				);

				cmd.end();

				// -------------------------------------------------------------
				// ФАЗА 7: Отправка на GPU (Submit) и Вывод на экран (Present)
				// -------------------------------------------------------------
				auto submitRes = activeResources.frameManager.submitRenderCommands(
					graphicsQueue,
					cmd,
					currentFrameIndex,
					imageIndex
				);
				if (submitRes != vk::Result::eSuccess) {
					throw std::runtime_error("Failed to submit command buffer to graphics queue");
				}

				auto presentResult = activeResources.frameManager.present(
					presentationQueue,
					*activeResources.swapchain.swapchain,
					imageIndex
				);

				// Обрабатываем возможный ресайз на этапе Present
				if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR) {
					recreateAllResources();
				} else if (presentResult != vk::Result::eSuccess) {
					throw std::runtime_error("Fatal: Failed to present rendered image to queue");
				}

				if (needsMadeScreenshoot) {
					safeScreenshot(allocator.getMappedPointer(*frameDatas[currentFrameIndex].screenshotBuffer), swapchain.extent.width, swapchain.extent.height);
					needsMadeScreenshoot = false;
				}

				// Переходим к следующему слоту кадра в полете
				currentFrameIndex = (currentFrameIndex + 1) % frameCount;
			}
			// Обрабатываем возможный ресайз на этапе Acquire
			else if (acquireResult.result == vk::Result::eSuboptimalKHR || acquireResult.result == vk::Result::eErrorOutOfDateKHR) {
				recreateAllResources();
			}
			else {
				throw std::runtime_error("Fatal: Failed to acquire next image from swapchain!");
			}
		}

		// Завершение работы
		if (uniqueDevice->waitIdle() != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to wait for device idle during shutdown");
		}
		uiRender.destroy(device);
		std::cout << "[Shutdown] Device idle confirmed. Exiting cleanly.\n";
		return 0;
	}
	catch (const std::exception& ex) {
		std::cerr << "[CRITICAL ERROR] " << ex.what() << '\n';
		return EXIT_FAILURE;
	}
}