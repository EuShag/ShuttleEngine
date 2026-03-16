#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "ImageLoader/Image.hpp"
#include "Sdl.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"
#include "VulkanInstanceBuilder/VulkanInstanceBuilder.hpp"


VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

int main(int argc, char** argv) {
	try{

		SdlLibrary sdlLibrary; // Ensure SDL is initialized and cleaned up properly
		VULKAN_HPP_DEFAULT_DISPATCHER.init();

		auto window = SdlWindow("Hello Triangle", 800, 600);

		window.setKeyboardEventCallback([&](SdlKeyCode keyCode, SdlKeyMode keyMode, SdlKeyState keyState) {
			if (keyCode == SdlKeyCode::Escape && keyState == SdlKeyState::Pressed) {
				std::cout << "Escape key pressed, closing window...\n";
				sdlLibrary.postQuitEvent();
			}});

		window.setWindowCloseEventCallback([&] {
			std::cout << "Window close event received, closing window...\n";
			sdlLibrary.postQuitEvent();
			});

		auto requiredInstanceExtensions = SdlLibrary::getSurfaceRequiredExtensions();

		requiredInstanceExtensions.push_back("VK_EXT_debug_utils");
		auto const validationLayers = std::vector{"VK_LAYER_KHRONOS_validation"};

		auto [buildInstanceResult, uniqueInstance] = 
			VulkanInstanceBuilder{[&](VulkanInstanceBuilder& self) {
				self
					.addExtensions(requiredInstanceExtensions)
					.addLayers(validationLayers)
					.setDebugMessenger(VulkanDebugger{})
					.setupApplicationInfo(vk::ApplicationInfo{
						.pApplicationName = "Hello Triangle",
						.applicationVersion = vk::makeVersion(1, 0, 0),
						.pEngineName = "No Engine",
						.engineVersion = vk::makeVersion(1, 0, 0),
						.apiVersion = vk::ApiVersion10
						});
			}}.buildUnique();

		if (buildInstanceResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create Vulkan instance");
		} VULKAN_HPP_DEFAULT_DISPATCHER.init(uniqueInstance.get());

		auto const uniqueSurface = window.createVulkanSurfaceUnique(*uniqueInstance);
	
		auto [enumeratePhysicalDevicesResult, physicalDevices] = uniqueInstance.get().enumeratePhysicalDevices();
		if (enumeratePhysicalDevicesResult != vk::Result::eSuccess || physicalDevices.empty()) {
			throw std::runtime_error("Failed to find a suitable GPU");
		}

		std::vector requiredDeviceExtensions = {
			vk::KHRSwapchainExtensionName
		};

		std::vector<uint32_t> graphicFamilyIndices{};
		std::vector<uint32_t> presentationFamilyIndices{};
		std::vector<uint32_t> computeFamilyIndices{};
		std::vector<uint32_t> transferFamilyIndices{};
		vk::PhysicalDevice physicalDevice = VK_NULL_HANDLE;

		for (auto physicalDevice1: physicalDevices) {
			bool requiredExtensionsSupported = checkExtensionsSupport(physicalDevice1, requiredDeviceExtensions);
			std::vector<uint32_t> physicalDevicePresentationFamilyIndices;
			std::vector<uint32_t> physicalDeviceGraphicFamilyIndices;
			std::vector<uint32_t> physicalDeviceComputeFamilyIndices;
			std::vector<uint32_t> physicalDeviceTransferFamilyIndices;
		
			auto physicalDeviceQueueFamilyProperties = physicalDevice1.getQueueFamilyProperties();

			for (int queueFamilyIndex = 0U; auto queueFamilyProperties: physicalDeviceQueueFamilyProperties) {
				if (queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) physicalDeviceGraphicFamilyIndices.push_back(queueFamilyIndex);
				if (queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute) physicalDeviceComputeFamilyIndices.push_back(queueFamilyIndex);
				if (queueFamilyProperties.queueFlags & vk::QueueFlagBits::eTransfer) physicalDeviceTransferFamilyIndices.push_back(queueFamilyIndex);
				if (physicalDevice1.getSurfaceSupportKHR(queueFamilyIndex, *uniqueSurface).value) physicalDevicePresentationFamilyIndices.push_back(queueFamilyIndex);
				queueFamilyIndex++;
			}

			if (requiredExtensionsSupported && !physicalDeviceGraphicFamilyIndices.empty() && !physicalDevicePresentationFamilyIndices.empty()) {
				physicalDevice = physicalDevice1;
				graphicFamilyIndices = physicalDeviceGraphicFamilyIndices;
				presentationFamilyIndices = physicalDevicePresentationFamilyIndices;
				computeFamilyIndices = physicalDeviceComputeFamilyIndices;
				transferFamilyIndices = physicalDeviceTransferFamilyIndices;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("Failed to find a suitable GPU with required extensions and queue families");
		}

		std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos{};
		std::array queuePriorities = {1.0f, 1.0f, 1.0f, 1.0f};
		std::pair<uint32_t, uint32_t> graphicQueueIndex{0,0};
		std::pair<uint32_t, uint32_t> presentationQueueIndex{0,0};
		std::pair<uint32_t, uint32_t> computeQueueIndex{0,0};
		std::pair<uint32_t, uint32_t> transferQueueIndex{0,0};

		for (auto graphicFamilyIndex: graphicFamilyIndices) {
			for (auto presentationFamilyIndex: presentationFamilyIndices) {
				for (auto computeFamilyIndex: computeFamilyIndices) {
					for (auto transferFamilyIndex: transferFamilyIndices) {
						if (graphicFamilyIndex == presentationFamilyIndex && presentationFamilyIndex == computeFamilyIndex && computeFamilyIndex == transferFamilyIndex) {
							deviceQueueCreateInfos.push_back(
								vk::DeviceQueueCreateInfo{
									.queueFamilyIndex = graphicFamilyIndex,
									.queueCount = 1,
									.pQueuePriorities = queuePriorities.data()
								}
							);
							graphicQueueIndex = { graphicFamilyIndex, 0 };
							presentationQueueIndex = { presentationFamilyIndex, 0 };
							computeQueueIndex = { computeFamilyIndex, 0 };
							transferQueueIndex = { transferFamilyIndex, 0 };
							break;
						}
					}
				}
			}
		}

		vk::PhysicalDeviceFeatures physicalDeviceFeatures{
			.samplerAnisotropy = vk::True
		};

		auto [createDeviceResult, uniqueDevice] = physicalDevice.createDeviceUnique(
			vk::DeviceCreateInfo{
				.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size()),
				.pQueueCreateInfos = deviceQueueCreateInfos.data(),
				.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
				.ppEnabledExtensionNames = requiredDeviceExtensions.data(),
				.pEnabledFeatures = &physicalDeviceFeatures
			});
		if (createDeviceResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create logical device");
		}

		vk::Queue graphicQueue = uniqueDevice->getQueue(graphicQueueIndex.first, graphicQueueIndex.second);
		vk::Queue presentationQueue = uniqueDevice->getQueue(presentationQueueIndex.first, presentationQueueIndex.second);
		vk::Queue computeQueue = uniqueDevice->getQueue(computeQueueIndex.first, computeQueueIndex.second);
		vk::Queue transferQueue = uniqueDevice->getQueue(transferQueueIndex.first, transferQueueIndex.second);

		auto [getSurfaceCapabilitiesResult, physicalDeviceSurfaceCapabilities] = physicalDevice.getSurfaceCapabilitiesKHR(uniqueSurface.get());
		if (getSurfaceCapabilitiesResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to get surface capabilities");
		}
		auto [getSurfaceFormatsResult, surfaceFormats] = physicalDevice.getSurfaceFormatsKHR(uniqueSurface.get());
		if (getSurfaceFormatsResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to get surface formats");
		}
		auto [getSurfacePresentModesResult, surfacePresentModes] = physicalDevice.getSurfacePresentModesKHR(uniqueSurface.get());
		if (getSurfacePresentModesResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to get surface present modes");
		}

		auto swapchainImageFormat = surfaceFormats.front().format;
		auto swapchainColorSpace = surfaceFormats.front().colorSpace;
		auto swapchainExtent = physicalDeviceSurfaceCapabilities.currentExtent;
		auto swapchainPresentMode = std::ranges::find(surfacePresentModes, vk::PresentModeKHR::eFifo) != surfacePresentModes.end() ? vk::PresentModeKHR::eFifo : surfacePresentModes.front();

		auto [createSwapchainResult, uniqueSwapchain] = uniqueDevice->createSwapchainKHRUnique(
			vk::SwapchainCreateInfoKHR{
				.surface = *uniqueSurface,
				.minImageCount = physicalDeviceSurfaceCapabilities.minImageCount,
				.imageFormat = swapchainImageFormat,
				.imageColorSpace = swapchainColorSpace,
				.imageExtent = swapchainExtent,
				.imageArrayLayers = 1,
				.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
				.imageSharingMode = vk::SharingMode::eExclusive,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr,
				.preTransform = physicalDeviceSurfaceCapabilities.currentTransform,
				.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
				.presentMode = swapchainPresentMode,
				.clipped = vk::True,
				.oldSwapchain = nullptr
			}
		);
		if (createSwapchainResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create swapchain");
		}

		auto [getSwapchainImagesResult, swapchainImages] = uniqueDevice->getSwapchainImagesKHR(*uniqueSwapchain);
		if (getSwapchainImagesResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to get swapchain images");
		}
		auto imageCount = static_cast<uint32_t>(swapchainImages.size());

		std::vector<vk::UniqueImageView> uniqueImageViews{};
		uniqueImageViews.reserve(swapchainImages.size());
		for (auto image: swapchainImages) {
			auto [createImageViewResult, uniqueImageView] = uniqueDevice->createImageViewUnique(
				vk::ImageViewCreateInfo{
					.image = image,
					.viewType = vk::ImageViewType::e2D,
					.format = swapchainImageFormat,
					.components = vk::ComponentMapping{
						.r = vk::ComponentSwizzle::eIdentity,
						.g = vk::ComponentSwizzle::eIdentity,
						.b = vk::ComponentSwizzle::eIdentity,
						.a = vk::ComponentSwizzle::eIdentity
					},
					.subresourceRange = vk::ImageSubresourceRange{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				});
			if (createImageViewResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create image view");
			}
			uniqueImageViews.push_back( std::move(uniqueImageView));
		}

		vk::AttachmentDescription swapchainAttachmentDescription{
			.format = swapchainImageFormat,
			.samples = vk::SampleCountFlagBits::e1,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::ePresentSrcKHR
		};

		vk::AttachmentReference swapchainAttachmentReference{
			.attachment = 0,
			.layout = vk::ImageLayout::eColorAttachmentOptimal
		};

		vk::SubpassDescription swapchainSubpassDescription{
			.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
			.colorAttachmentCount = 1,
			.pColorAttachments = &swapchainAttachmentReference
		};

		std::array swapchainSubpassDependencies{
			vk::SubpassDependency{
				.srcSubpass = vk::SubpassExternal,
				.dstSubpass = 0,
				.srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe,
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

		auto [renderPassCreateResult, uniqueRenderPass] = uniqueDevice->createRenderPassUnique(
			vk::RenderPassCreateInfo{
				.attachmentCount = 1,
				.pAttachments = &swapchainAttachmentDescription,
				.subpassCount = 1,
				.pSubpasses = &swapchainSubpassDescription,
				.dependencyCount = static_cast<uint32_t>(swapchainSubpassDependencies.size()),
				.pDependencies = swapchainSubpassDependencies.data()
			}
		);
		if (renderPassCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create render pass");
		}

		std::vector<vk::UniqueFramebuffer> uniqueFramebuffers{};
		uniqueFramebuffers.reserve(imageCount);
		for (auto& imageView: uniqueImageViews) {
			auto [createFramebufferResult, uniqueFramebuffer] = uniqueDevice->createFramebufferUnique(
				vk::FramebufferCreateInfo{
					.renderPass = *uniqueRenderPass,
					.attachmentCount = 1,
					.pAttachments = &imageView.get(),
					.width = swapchainExtent.width,
					.height = swapchainExtent.height,
					.layers = 1
				}
			);
			if (createFramebufferResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create framebuffer");
			}
			uniqueFramebuffers.push_back(std::move(uniqueFramebuffer));
		}

		std::array descriptorSetLayoutBindings{
			vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment
			},
			vk::DescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex
			}
		};

		auto [descriptorSetLayoutCreateResult, uniqueDescriptorSetLayout] = uniqueDevice->createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo{
				.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size()),
				.pBindings = descriptorSetLayoutBindings.data()
			}
		);
		if (descriptorSetLayoutCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create descriptor set layout");
		}

		std::array descriptorSizes{
			vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1
			},
			vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1
			}
		};

		auto [descriptorPoolCreateResult, uniqueDescriptorPool] = uniqueDevice->createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo{
				.maxSets = 1,
				.poolSizeCount = static_cast<uint32_t>(descriptorSizes.size()),
				.pPoolSizes = descriptorSizes.data(),
			}
		);
		if (descriptorPoolCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create descriptor pool");
		}

		auto [descriptorSetAllocateResult, uniqueDescriptorSets] = uniqueDevice->allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo{
				.descriptorPool = *uniqueDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &uniqueDescriptorSetLayout.get()
			}
		);
		if (descriptorSetAllocateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate descriptor set");
		}
		auto& uniqueDescriptorSet = uniqueDescriptorSets.front();

		struct CameraData {
			glm::mat4 view;
			glm::mat4 projection;
		} cameraData;

		auto [cameraBufferCreateResult, uniqueCameraBuffer] = uniqueDevice->createBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(CameraData),
				.usage = vk::BufferUsageFlagBits::eUniformBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			}
		);
		if (cameraBufferCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create camera uniform buffer");
		}

		CubeMapImageFiles files{
			.rightFilePath = "img/right.jpg",
			.leftFilePath = "img/left.jpg",
			.topFilePath = "img/top.jpg",
			.bottomFilePath = "img/bottom.jpg",
			.backFilePath = "img/back.jpg",
			.frontFilePath = "img/front.jpg"
		};

		CubeMapImage{files};

		auto cubeMapImageData = CubeMapImage{ files }.getImageData();

		auto sideWidth = static_cast<uint32_t>(cubeMapImageData.sideWidth);

		auto [stagingBufferCreateResult, uniqueStagingBuffer] = uniqueDevice->createBufferUnique(
			vk::BufferCreateInfo{
				.size = static_cast<vk::DeviceSize>(sideWidth) * 4 * 6, // Assuming 4 bytes per pixel (RGBA) and 6 faces
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			}
		);
		if (stagingBufferCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create staging buffer");
		}

		auto stagingBufferMemoryRequirements = uniqueDevice->getBufferMemoryRequirements(*uniqueStagingBuffer);

		auto [allocateStagingBufferMemoryResult, stagingBufferMemory] = uniqueDevice->allocateMemoryUnique(
			vk::MemoryAllocateInfo{
				.allocationSize = stagingBufferMemoryRequirements.size,
				.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, stagingBufferMemoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
			}
		);
		if (allocateStagingBufferMemoryResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate memory for staging buffer");
		}

		if (auto stagingBufferBindResult = uniqueDevice->bindBufferMemory(*uniqueStagingBuffer, *stagingBufferMemory, 0); stagingBufferBindResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to bind memory to staging buffer");
		}

		auto [mapStagingBufferMemoryResult, mappedStagingBufferMemory] = uniqueDevice->mapMemory(*stagingBufferMemory, 0, static_cast<vk::DeviceSize>(sideWidth) * 4 * 6);
		if (mapStagingBufferMemoryResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to map staging buffer memory");
		}

		std::memcpy(mappedStagingBufferMemory, cubeMapImageData.leftData, static_cast<size_t>(sideWidth) * 4);
		std::memcpy(static_cast<char*>(mappedStagingBufferMemory) + static_cast<size_t>(sideWidth) * 4, cubeMapImageData.rightData, static_cast<size_t>(sideWidth) * 4);
		std::memcpy(static_cast<char*>(mappedStagingBufferMemory) + static_cast<size_t>(sideWidth) * 4 * 2, cubeMapImageData.topData, static_cast<size_t>(sideWidth) * 4);
		std::memcpy(static_cast<char*>(mappedStagingBufferMemory) + static_cast<size_t>(sideWidth) * 4 * 3, cubeMapImageData.bottomData, static_cast<size_t>(sideWidth) * 4);
		std::memcpy(static_cast<char*>(mappedStagingBufferMemory) + static_cast<size_t>(sideWidth) * 4 * 4, cubeMapImageData.backData, static_cast<size_t>(sideWidth) * 4);
		std::memcpy(static_cast<char*>(mappedStagingBufferMemory) + static_cast<size_t>(sideWidth) * 4 * 5, cubeMapImageData.frontData, static_cast<size_t>(sideWidth) * 4);

		auto [cubeMapImageCreateResult, uniqueCubeMapImage] = uniqueDevice->createImageUnique(
			vk::ImageCreateInfo{
				.flags = vk::ImageCreateFlagBits::eCubeCompatible,
				.imageType = vk::ImageType::e2D,
				.format = vk::Format::eR8G8B8A8Srgb,
				.extent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1 },
				.mipLevels = 1,
				.arrayLayers = 6,
				.samples = vk::SampleCountFlagBits::e1,
				.tiling = vk::ImageTiling::eOptimal,
				.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				.sharingMode = vk::SharingMode::eExclusive,
				.initialLayout = vk::ImageLayout::eUndefined
			}
		);
		if (cubeMapImageCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube map image");
		}

		auto cubeMapImageMemoryRequirements = uniqueDevice->getImageMemoryRequirements(*uniqueCubeMapImage);

		auto [allocateImageMemoryResult, ImageMemory] = uniqueDevice->allocateMemoryUnique(
			vk::MemoryAllocateInfo{
				.allocationSize = cubeMapImageMemoryRequirements.size,
				.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, cubeMapImageMemoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
			}
		);
		if (allocateImageMemoryResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate memory for cube map image");
		}

		if (auto imageBindResult = uniqueDevice->bindImageMemory(*uniqueCubeMapImage, *ImageMemory, 0); imageBindResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to bind memory to cube map image");
		}

		auto [createCopyCommandPoolResult, uniqueCopyCommandPool] = uniqueDevice->createCommandPoolUnique(
			vk::CommandPoolCreateInfo{
				.flags = vk::CommandPoolCreateFlagBits::eTransient,
				.queueFamilyIndex = transferQueueIndex.first
			}
		);
		if (createCopyCommandPoolResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create command pool for copy operations");
		}

		auto [copyCommandBufferAllocateResult, uniqueCopyCommandBuffer] = uniqueDevice->allocateCommandBuffersUnique(
			vk::CommandBufferAllocateInfo{
				.commandPool = *uniqueCopyCommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1
			}
		);
		if (copyCommandBufferAllocateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate command buffer for copy operations");
		}

		// Transition cube map image layout to be optimal for receiving data from the staging buffer
		auto copyCommandBuffer = uniqueCopyCommandBuffer.front().get();
		if (auto beginCopyCommandBufferResult = copyCommandBuffer.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit }); beginCopyCommandBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to begin recording copy command buffer");
		}
		copyCommandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			{},
			{},
			vk::ImageMemoryBarrier{
				.srcAccessMask = {},
				.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eTransferDstOptimal,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = *uniqueCubeMapImage,
				.subresourceRange = vk::ImageSubresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 6
				}
			}
		);

		std::array bufferToImageRegions{
			vk::BufferImageCopy{
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 6
				},
				.imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
				.imageExtent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1}
			},
			vk::BufferImageCopy{
				.bufferOffset = static_cast<vk::DeviceSize>(sideWidth) * 4,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 1,
					.layerCount = 1
				},
				.imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
				.imageExtent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1}
			},
			vk::BufferImageCopy{
				.bufferOffset = static_cast<vk::DeviceSize>(sideWidth) * 4 * 2,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 2,
					.layerCount = 1
				},
				.imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
				.imageExtent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1}
			},
			vk::BufferImageCopy{
				.bufferOffset = static_cast<vk::DeviceSize>(sideWidth) * 4 * 3,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 3,
					.layerCount = 1
				},
				.imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
				.imageExtent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1}
			},
			vk::BufferImageCopy{
				.bufferOffset = static_cast<vk::DeviceSize>(sideWidth) * 4 * 4,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 4,
					.layerCount = 1
				},
				.imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
				.imageExtent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1}
			},
			vk::BufferImageCopy{
				.bufferOffset = static_cast<vk::DeviceSize>(sideWidth) * 4 * 5,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = 5,
					.layerCount = 1
				},
				.imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
				.imageExtent = vk::Extent3D{.width = sideWidth, .height = sideWidth, .depth = 1}
			}
		};

		copyCommandBuffer.copyBufferToImage(
			*uniqueStagingBuffer,
			*uniqueCubeMapImage,
			vk::ImageLayout::eTransferDstOptimal,
			bufferToImageRegions
		);
		copyCommandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			vk::ImageMemoryBarrier{
				.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
				.dstAccessMask = vk::AccessFlagBits::eShaderRead,
				.oldLayout = vk::ImageLayout::eTransferDstOptimal,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = *uniqueCubeMapImage,
				.subresourceRange = vk::ImageSubresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 6
				}
			}
		);
		if (auto endCopyCommandBufferResult = copyCommandBuffer.end(); endCopyCommandBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to end recording copy command buffer");
		}

		auto [transferSubmitFenceCreateResult, uniqueTransferFence] = uniqueDevice->createFenceUnique(vk::FenceCreateInfo{});
		if (transferSubmitFenceCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create fence for copy command buffer submission");
		}

		auto transferSubmitResult = transferQueue.submit(
			vk::SubmitInfo{
				.commandBufferCount = 1,
				.pCommandBuffers = &copyCommandBuffer
			},
			*uniqueTransferFence
		);
		if (transferSubmitResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to submit copy command buffer");
		}

		if (auto waitTransferFenceResult = uniqueDevice->waitForFences(*uniqueTransferFence, vk::True, UINT64_MAX); waitTransferFenceResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to wait for copy command buffer fence");
		}

		auto [cubeMapImageViewCreateResult, uniqueCubeMapImageView] = uniqueDevice->createImageViewUnique(
			vk::ImageViewCreateInfo{
				.image = *uniqueCubeMapImage,
				.viewType = vk::ImageViewType::eCube,
				.format = vk::Format::eR8G8B8A8Srgb,
				.components = vk::ComponentMapping{
					.r = vk::ComponentSwizzle::eIdentity,
					.g = vk::ComponentSwizzle::eIdentity,
					.b = vk::ComponentSwizzle::eIdentity,
					.a = vk::ComponentSwizzle::eIdentity
				},
				.subresourceRange = vk::ImageSubresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 6
				}
			}
		);
		if (cubeMapImageViewCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube map image view");
		}

		auto [cubeMapSamplerCreateResult, uniqueCubeMapSampler] = uniqueDevice->createSamplerUnique(
			vk::SamplerCreateInfo{
				.magFilter = vk::Filter::eLinear,
				.minFilter = vk::Filter::eLinear,
				.mipmapMode = vk::SamplerMipmapMode::eLinear,
				.addressModeU = vk::SamplerAddressMode::eRepeat,
				.addressModeV = vk::SamplerAddressMode::eRepeat,
				.addressModeW = vk::SamplerAddressMode::eRepeat,
				.mipLodBias = 0.0f,
				.anisotropyEnable = vk::True,
				.maxAnisotropy = 16.0f,
				.compareEnable = vk::False,
				.compareOp = vk::CompareOp::eAlways,
				.minLod = 0.0f,
				.maxLod = 0.0f,
				.borderColor = vk::BorderColor::eIntOpaqueBlack,
				.unnormalizedCoordinates = vk::False
			}
		);
		if (cubeMapSamplerCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube map sampler");
		}

		vk::DescriptorImageInfo cubeMapDescriptorImageInfo{
			.sampler = *uniqueCubeMapSampler,
			.imageView = *uniqueCubeMapImageView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		vk::DescriptorBufferInfo cameraBufferDescriptorInfo{
			.buffer = *uniqueCameraBuffer,
			.offset = 0,
			.range = sizeof(CameraData)
		};

		std::array writeDescriptorSets{
			vk::WriteDescriptorSet{
				.dstSet = *uniqueDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &cubeMapDescriptorImageInfo
			},
			vk::WriteDescriptorSet{
				.dstSet = *uniqueDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &cameraBufferDescriptorInfo // To be filled later with actual uniform buffer info
			}
		};

		uniqueDevice->updateDescriptorSets(
			writeDescriptorSets,
			{}
		);

		std::array triangleVertices{ 
			// positions|colors
			0.0f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
		   -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
	   };

		struct CubeVertex {
			glm::vec3 position;
			glm::vec3 directionVector;
		};

		// vertices for a cube;
		std::array cubeVertices{
			CubeVertex{.position{-0.5f, -0.5f, -0.5f}, .directionVector{-1.0f, -1.0f, -1.0f} },
			CubeVertex{.position{ 0.5f, -0.5f, -0.5f}, .directionVector{ 1.0f, -1.0f, -1.0f} },
			CubeVertex{.position{ 0.5f,  0.5f, -0.5f}, .directionVector{ 1.0f,  1.0f, -1.0f} },
			CubeVertex{.position{-0.5f,  0.5f, -0.5f}, .directionVector{-1.0f,  1.0f, -1.0f} },
			CubeVertex{.position{-0.5f, -0.5f,  0.5f}, .directionVector{-1.0f, -1.0f,  1.0f} },
			CubeVertex{.position{ 0.5f, -0.5f,  0.5f}, .directionVector{ 1.0f, -1.0f,  1.0f} },
			CubeVertex{.position{ 0.5f,  0.5f,  0.5f}, .directionVector{ 1.0f,  1.0f,  1.0f} },
			CubeVertex{.position{-0.5f,  0.5f,  0.5f}, .directionVector{-1.0f,  1.0f,  1.0f} }
		};

		struct ModelData {
			glm::mat4 model;
		};

		ModelData cubePosition{
			.model{glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f))}
		};



		// Write indices for the cube vertices
		std::array cubeIndices{
			0, 1, 2, 2, 3, 0, // back face
			4, 5, 6, 6, 7, 4, // front face
			4, 5, 1, 1, 0, 4, // bottom face
			7, 6, 2, 2, 3, 7, // top face
			4, 7, 3, 3, 0, 4, // left face
			5, 6, 2, 2, 1, 5  // right face
		};
		
		auto [createCubeVertexBufferResult, uniqueCubeVertexBuffer] = uniqueDevice->createBufferUnique(
			vk::BufferCreateInfo{
			.size = sizeof(cubeVertices),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = vk::SharingMode::eExclusive
			});
		if (createCubeVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube vertex buffer");
		}

		auto [createCubeIndexBufferResult, uniqueCubeIndexBuffer] = uniqueDevice->createBufferUnique(
			vk::BufferCreateInfo{
			.size = sizeof(cubeIndices),
			.usage = vk::BufferUsageFlagBits::eIndexBuffer,
			.sharingMode = vk::SharingMode::eExclusive
			});
		if (createCubeIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube index buffer");
		}




		auto [createBufferResult, uniqueVertexBuffer] = uniqueDevice->createBufferUnique(
			vk::BufferCreateInfo{
			.size = sizeof(triangleVertices),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = vk::SharingMode::eExclusive
			});
		if (createBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create vertex buffer");
		}

		auto vertexBufferMemoryRequirements = uniqueDevice->getBufferMemoryRequirements(*uniqueVertexBuffer);
		auto memoryProperties = physicalDevice.getMemoryProperties();

		for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++) {
			std::cout << "Memory type " << memoryTypeIndex << ": "
				<< ((memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) ? "Host Visible " : "")
				<< ((memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) ? "Host Coherent " : "")
				<< ((memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) ? "Device Local " : "")
				<< ((memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::MemoryPropertyFlagBits::eHostCached) ? "Host Cached " : "")
				<< '\n';
		}

		std::optional<uint32_t> vertexBufferMemoryTypeIndex{0};
		for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++) {
			if (vertexBufferMemoryRequirements.memoryTypeBits & 1 << memoryTypeIndex &&
				memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible &&
				memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) {
				vertexBufferMemoryTypeIndex = memoryTypeIndex;
				break;
				}
		}
		if (!vertexBufferMemoryTypeIndex.has_value()) {
			throw std::runtime_error("Failed to find suitable memory type for vertex buffer");
		}

		auto [allocateMemoryResult, uniqueVertexBufferMemory] = uniqueDevice->allocateMemoryUnique(
			vk::MemoryAllocateInfo{
				.allocationSize = vertexBufferMemoryRequirements.size,
				.memoryTypeIndex = vertexBufferMemoryTypeIndex.value()
			}
		);
		if (allocateMemoryResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate vertex buffer memory");
		}

		if (auto allocateVertexBufferMemoryResult = uniqueDevice->bindBufferMemory(*uniqueVertexBuffer, *uniqueVertexBufferMemory, 0); allocateVertexBufferMemoryResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to bind vertex buffer memory");
		}

		// Map memory and copy vertex data
		auto [mapMemoryResult, vertexBufferData] = uniqueDevice->mapMemory(uniqueVertexBufferMemory.get(), 0, vertexBufferMemoryRequirements.size);
		if (mapMemoryResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to map vertex buffer memory");
		}
		std::memcpy(vertexBufferData, triangleVertices.data(), sizeof(triangleVertices));
		uniqueDevice->unmapMemory(uniqueVertexBufferMemory.get());

		auto [vertexShaderModuleCreateResult, uniqueVertexShaderModule] = loadAndCreateShaderModule(*uniqueDevice, vk::PipelineStageFlagBits::eVertexShader, "shaders/triangle.vert.spv");
		if (vertexShaderModuleCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create vertex shader module");
		}

		auto [fragmentShaderModuleCreateResult, uniqueFragmentShaderModule] = loadAndCreateShaderModule(*uniqueDevice, vk::PipelineStageFlagBits::eFragmentShader, "shaders/triangle.frag.spv");
		if (fragmentShaderModuleCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create fragment shader module");
		}

		std::vector shaderStages = {
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *uniqueVertexShaderModule,
				.pName = "main"
			},
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *uniqueFragmentShaderModule,
				.pName = "main"
			}
		};

		auto [createPipelineLayoutResult, uniquePipelineLayout] = uniqueDevice->createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo{}
		);
		if (createPipelineLayoutResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create pipeline layout");
		}

		vk::VertexInputBindingDescription vertexInputBinding{
			.binding = 0,
			.stride = sizeof(float) * 5,
			.inputRate = vk::VertexInputRate::eVertex
		};

		vk::VertexInputAttributeDescription vertexInputAttributes[2]{
			{
				.location = 0,
				.binding = 0,
				.format = vk::Format::eR32G32Sfloat,
				.offset = 0
			},
			{
				.location = 1,
				.binding = 0,
				.format = vk::Format::eR32G32B32Sfloat,
				.offset = 2 * sizeof(float)
			}
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertexInputBinding,
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = vertexInputAttributes
		};

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};

		vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
			.depthClampEnable = vk::False ,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eClockwise,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f
		};

		vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False
		};

		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
			.depthTestEnable = vk::False,
			.stencilTestEnable = vk::False
		};

		vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR |
							  vk::ColorComponentFlagBits::eG |
							  vk::ColorComponentFlagBits::eB |
							  vk::ColorComponentFlagBits::eA
		};

		vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
			.logicOpEnable = vk::False,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachmentState
		};

		vk::Viewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapchainExtent.width),
			.height = static_cast<float>(swapchainExtent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		vk::Rect2D scissor{
			.offset = vk::Offset2D{0, 0},
			.extent = swapchainExtent
		};

		vk::PipelineViewportStateCreateInfo viewPortStateCreateInfo{
			.viewportCount = 1,
			.pViewports = &viewport,
			.scissorCount = 1,
			.pScissors = &scissor
		};

		auto [uniqueGraphicsPipelineResult, uniqueGraphicsPipeline] = uniqueDevice->createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo{
				.stageCount = static_cast<uint32_t>(shaderStages.size()),
				.pStages = shaderStages.data(),
				.pVertexInputState = &vertexInputStateCreateInfo,
				.pInputAssemblyState = &inputAssemblyStateCreateInfo,
				.pViewportState = &viewPortStateCreateInfo,
				.pRasterizationState = &rasterizationStateCreateInfo,
				.pMultisampleState = &multisampleStateCreateInfo,
				.pDepthStencilState = &depthStencilStateCreateInfo,
				.pColorBlendState = &colorBlendStateCreateInfo,
				.layout = uniquePipelineLayout.get(),
				.renderPass = uniqueRenderPass.get(),
				.subpass = 0
			}
		);
		if (uniqueGraphicsPipelineResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create graphics pipeline.");
		}

		auto [createCommandPoolResult, uniqueCommandPool] = uniqueDevice->createCommandPoolUnique(
			vk::CommandPoolCreateInfo{
				.queueFamilyIndex = graphicQueueIndex.first
			}
		);
		if (createCommandPoolResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create command pool");
		}

		auto [allocateCommandBuffersResult, uniqueCommandBuffer] = uniqueDevice->allocateCommandBuffersUnique(
			vk::CommandBufferAllocateInfo{
				.commandPool = *uniqueCommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = imageCount
			}
		);
		if (allocateCommandBuffersResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate command buffers");
		}

		vk::ClearValue clearValue{
			.color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
		};

		for (uint32_t frameIndex = 0U; auto& commandBuffer : uniqueCommandBuffer) {
			if (auto beginCommandBufferResult = commandBuffer->begin(vk::CommandBufferBeginInfo{
				.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse
			}); beginCommandBufferResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to begin recording command buffer");
			}
			commandBuffer->beginRenderPass(
				vk::RenderPassBeginInfo{
					.renderPass = *uniqueRenderPass,
					.framebuffer = uniqueFramebuffers[frameIndex].get(),
					.renderArea = vk::Rect2D{
						.offset = vk::Offset2D{ .x = 0, .y = 0},
						.extent = swapchainExtent
					},
					.clearValueCount = 1,
					.pClearValues = &clearValue
				},
				vk::SubpassContents::eInline
			);
			commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *uniqueGraphicsPipeline);
			commandBuffer->bindVertexBuffers(0, {*uniqueVertexBuffer}, {0});
			commandBuffer->draw(triangleVertices.size() / 5, 1, 0, 0);
			commandBuffer->endRenderPass();
			if (auto endCommandBufferResult = commandBuffer->end(); endCommandBufferResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to end recording command buffer");
			}
			frameIndex++;
		}

		// Main loop
		// In the main loop
		bool running = true;
		uint32_t currentFrame = 0U;
		std::vector<vk::UniqueSemaphore> imageAvailableSemaphores{};
		std::vector<vk::UniqueSemaphore> renderFinishedSemaphores{};
		std::vector<vk::UniqueFence> inFlightFences{};
		imageAvailableSemaphores.reserve(imageCount);
		renderFinishedSemaphores.reserve(imageCount);
		inFlightFences.reserve(imageCount);
		for (uint32_t i = 0; i < imageCount; i++) {
			auto [createImageAvailableSemaphoreResult, imageAvailableSemaphore] = uniqueDevice->createSemaphoreUnique({});
			if (createImageAvailableSemaphoreResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create image available semaphore");
			}
			imageAvailableSemaphores.push_back(std::move(imageAvailableSemaphore));
			auto [createRenderFinishedSemaphoreResult, renderFinishedSemaphore] = uniqueDevice->createSemaphoreUnique({});
			if (createRenderFinishedSemaphoreResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create render finished semaphore");
			}
			renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
			auto [createInFlightFenceResult, inFlightFence] = uniqueDevice->createFenceUnique(vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
			if (createInFlightFenceResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create in-flight fence");
			}
			inFlightFences.push_back(std::move(inFlightFence));
		}

		vk::PipelineStageFlags pipelineStageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;


		while (running){

			if(!sdlLibrary.pullEvents()) break;

			if (auto waitFenceResult = uniqueDevice->waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX); waitFenceResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to wait for fence");
			}
			if (auto resetFenceResult = uniqueDevice->resetFences(*inFlightFences[currentFrame]); resetFenceResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to reset fence");
			}

			auto [acquireNextImageResult, currentImageIndex] = uniqueDevice->acquireNextImageKHR(*uniqueSwapchain, UINT64_MAX, *imageAvailableSemaphores[currentFrame], nullptr);
			if (acquireNextImageResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to acquire next image");
			}

			auto submitResult = graphicQueue.submit(
				{ vk::SubmitInfo{
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &imageAvailableSemaphores[currentFrame].get(),
					.pWaitDstStageMask = &pipelineStageFlags,
					.commandBufferCount = 1,
					.pCommandBuffers = &uniqueCommandBuffer[currentImageIndex].get(),
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = &renderFinishedSemaphores[currentFrame].get()
				} }, inFlightFences[currentFrame].get());
			if (submitResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to submit draw command buffer");
			}
			auto presentResult = presentationQueue.presentKHR(
				vk::PresentInfoKHR{
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &renderFinishedSemaphores[currentFrame].get(),
					.swapchainCount = 1,
					.pSwapchains = &uniqueSwapchain.get(),
					.pImageIndices = &currentImageIndex
				}
			);
			if (presentResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to present swapchain image");
			}
			currentFrame = currentFrame + 1 < imageCount ? currentFrame + 1 : 0;
		}
		if (auto waitIdleResult = uniqueDevice->waitIdle(); waitIdleResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to wait for device idle");
		}
		return 0;
	}
	catch (std::exception& ex) {
		std::cerr << "Error: " << ex.what() << '\n';
		return EXIT_FAILURE;
	}
}