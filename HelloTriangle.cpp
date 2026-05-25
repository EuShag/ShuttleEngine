#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <fstream>
#include <iostream>
#include <queue>
#include <stack>
#include <utility>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "ImageLoader/Image.hpp"
#include "Sdl.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"
#include "VulkanInstanceBuilder/VulkanInstanceBuilder.hpp"
#include "VulkanDeviceAllocator/VulkanDeviceAllocator.hpp"
#include "VulkanCopyManager/VulkanCopyManager.hpp"
#include "Camera/Camera.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/matrix4x4.h>
#include <glm/gtc/type_ptr.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// Функция для обхода aiNode в ширину
void traverseNode(aiNode* node, std::function<void(aiNode*)> const& callback) {
	std::stack<aiNode*> nodeStack;
	nodeStack.push(node);
	while (!nodeStack.empty()) {
		auto currentNode = nodeStack.top();
		nodeStack.pop();

		callback(currentNode);

		for (unsigned int i = 0; i < currentNode->mNumChildren; ++i) {
			nodeStack.push(currentNode->mChildren[i]);
		}
	}
}

int main(int argc, char** argv) {
	try {
		Assimp::Importer importer;
		auto pScene = importer.ReadFile("assets/models/lowe/scene.gltf", aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
		if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode) {
			std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
			return -1;
		}

		aiNode* loweNode = nullptr;
		auto modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));

		traverseNode(pScene->mRootNode, [&](aiNode* node) {
			std::cout << node->mName.C_Str() << std::endl;
			if (std::string(node->mName.C_Str()) == "Loewe_C.obj.cleaner.materialmerger.gles") {
				loweNode = node;
			}
			modelMatrix = modelMatrix *	glm::transpose(glm::make_mat4(&node->mTransformation.a1));
		});

		std::vector<aiMesh*> loweMeshes;

		for (unsigned int i = 0; i < loweNode->mNumChildren; ++i) {
			auto meshNode = loweNode->mChildren[i];
			// display transformation matrix
			for (unsigned int j = 0; j < meshNode->mNumMeshes; ++j) {
				auto meshIndex = meshNode->mMeshes[j];
				loweMeshes.push_back(pScene->mMeshes[meshIndex]);
			}
		}

		struct DrawCallInfo {
			uint32_t indexCount;
			uint32_t firstIndex;
			int32_t vertexOffset;
		};

		std::vector<DrawCallInfo> drawCalls;

		size_t vertexBufferTotalCount = 0;
		size_t indexBufferTotalCount = 0;


		for (auto mesh: loweMeshes) {
			vertexBufferTotalCount += mesh->mNumVertices;
			indexBufferTotalCount += mesh->mNumFaces * 3;
		}

		size_t vertexTexCoordAttributeInitialOffset = vertexBufferTotalCount * sizeof(float) * 3;
		size_t vertexBufferSize = vertexBufferTotalCount * (sizeof(float) * 3 + sizeof(float) * 2);
		size_t indexBufferSize = indexBufferTotalCount * sizeof(uint32_t);

		vm::Camera camera{ glm::vec3{0.3,0.3,0.3}, glm::quat{0.1, 0.2, 0.2, 0.2} };
		camera.lookAt(glm::vec3{ 0.0f, 0.0f, 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });

		SdlLibrary sdlLibrary; // Ensure SDL is initialized and cleaned up properly
		VULKAN_HPP_DEFAULT_DISPATCHER.init();

		auto window = SdlWindow("Hello Triangle", 1800, 1000);

		window.setWindowCloseEventCallback([&] (SdlWindow&) {
			std::cout << "Window close event received, closing window...\n";
			sdlLibrary.postQuitEvent();
			});

		auto requiredInstanceExtensions = SdlLibrary::getSurfaceRequiredExtensions();

		requiredInstanceExtensions.push_back("VK_EXT_debug_utils");
		auto const validationLayers = std::vector{ "VK_LAYER_KHRONOS_validation" };

		auto [buildInstanceResult, uniqueInstance] =
			VulkanInstanceBuilder{ [&](VulkanInstanceBuilder& self) {
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
			} }.buildUnique();

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

		for (auto physicalDevice1 : physicalDevices) {
			bool requiredExtensionsSupported = checkExtensionsSupport(physicalDevice1, requiredDeviceExtensions);
			std::vector<uint32_t> physicalDevicePresentationFamilyIndices;
			std::vector<uint32_t> physicalDeviceGraphicFamilyIndices;
			std::vector<uint32_t> physicalDeviceComputeFamilyIndices;
			std::vector<uint32_t> physicalDeviceTransferFamilyIndices;

			auto physicalDeviceQueueFamilyProperties = physicalDevice1.getQueueFamilyProperties();

			for (int queueFamilyIndex = 0U; auto&& queueFamilyProperties: physicalDeviceQueueFamilyProperties) {
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

		auto memoryProperties = physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
			auto memoryType = memoryProperties.memoryTypes[i];
			std::cout << "MemoryType" << i << " :" << to_string(memoryProperties.memoryTypes[i].propertyFlags) << std::endl;
		}

		std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos{};
		std::array queuePriorities = { 1.0f, 1.0f, 1.0f, 1.0f };
		std::pair<uint32_t, uint32_t> graphicQueueIndex{ 0,0 };
		std::pair<uint32_t, uint32_t> presentationQueueIndex{ 0,0 };
		std::pair<uint32_t, uint32_t> computeQueueIndex{ 0,0 };
		std::pair<uint32_t, uint32_t> transferQueueIndex{ 0,0 };

		for (auto graphicFamilyIndex : graphicFamilyIndices) {
			for (auto presentationFamilyIndex : presentationFamilyIndices) {
				for (auto computeFamilyIndex : computeFamilyIndices) {
					for (auto transferFamilyIndex : transferFamilyIndices) {
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

		auto physicalDeviceVersion = physicalDevice.getProperties().apiVersion;
		std::cout << "Physical device version: " << VK_API_VERSION_MAJOR(physicalDeviceVersion) << "."
												 << VK_API_VERSION_MINOR(physicalDeviceVersion ) << "."
												 << VK_API_VERSION_PATCH(physicalDeviceVersion) << std::endl;

		auto [createAllocatorResult, uniqueAllocator] = vma::UniqueAllocator::makeUnique(
			*uniqueInstance,
			*uniqueDevice,
			physicalDevice,
			vk::detail::defaultDispatchLoaderDynamic
		);
		if (createAllocatorResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create VMA allocator");
		}

		vk::Queue graphicQueue = uniqueDevice->getQueue(graphicQueueIndex.first, graphicQueueIndex.second);
		vk::Queue presentationQueue = uniqueDevice->getQueue(presentationQueueIndex.first, presentationQueueIndex.second);
		vk::Queue computeQueue = uniqueDevice->getQueue(computeQueueIndex.first, computeQueueIndex.second);
		vk::Queue transferQueue = uniqueDevice->getQueue(transferQueueIndex.first, transferQueueIndex.second);

		auto [createUniqueCommandPoolResult, uniqueTransferCommandPool] = uniqueDevice->createCommandPoolUnique(
			vk::CommandPoolCreateInfo{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = transferQueueIndex.first
			}
		);
		if (createUniqueCommandPoolResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create command pool");
		}

		auto [allocateUniqueCommandBuffersResult, uniqueTransferCommandBuffers] = uniqueDevice->allocateCommandBuffersUnique(
			vk::CommandBufferAllocateInfo{
				.commandPool = *uniqueTransferCommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 2
			}
		);
		if (allocateUniqueCommandBuffersResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate command buffers");
		}
		auto cubeMapCopyCommandBuffer = uniqueTransferCommandBuffers[0].get();
		auto texture2dCopyCommandBuffer = uniqueTransferCommandBuffers[1].get();

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
			uniqueImageViews.push_back(std::move(uniqueImageView));
		}

		auto [createDepthBufferResult, uniqueDepthBufferImage] = uniqueAllocator->createAndAllocateImageUnique(
			vk::ImageCreateInfo{
				.imageType = vk::ImageType::e2D,
				.format = vk::Format::eD32SfloatS8Uint,
				.extent = vk::Extent3D{
					.width = swapchainExtent.width,
					.height = swapchainExtent.height,
					.depth = 1
				},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = vk::SampleCountFlagBits::e1,
				.tiling = vk::ImageTiling::eOptimal,
				.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.sharingMode = vk::SharingMode::eExclusive,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr,
				.initialLayout = vk::ImageLayout::eUndefined
			},
			vma::MemoryUsage::ePreferDeviceMemory
		);
		if (createDepthBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create depth buffer image");
		}

		auto [createDepthBufferImageViewResult, uniqueDepthBufferImageView] = uniqueDevice->createImageViewUnique(
			vk::ImageViewCreateInfo{
				.image = *uniqueDepthBufferImage,
				.viewType = vk::ImageViewType::e2D,
				.format = vk::Format::eD32SfloatS8Uint,
				.components = vk::ComponentMapping{
					.r = vk::ComponentSwizzle::eIdentity,
					.g = vk::ComponentSwizzle::eIdentity,
					.b = vk::ComponentSwizzle::eIdentity,
					.a = vk::ComponentSwizzle::eIdentity
				},
				.subresourceRange = vk::ImageSubresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eDepth,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			});
		if (createDepthBufferImageViewResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create depth buffer image view");
		}

		std::array swapchainAttachmentDescriptions{
			vk::AttachmentDescription{
				.format = swapchainImageFormat,
				.samples = vk::SampleCountFlagBits::e1,
				.loadOp = vk::AttachmentLoadOp::eClear,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
				.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
				.initialLayout = vk::ImageLayout::eUndefined,
				.finalLayout = vk::ImageLayout::ePresentSrcKHR
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

		vk::AttachmentReference colorAttachmentReference{
			.attachment = 0,
			.layout = vk::ImageLayout::eColorAttachmentOptimal
		};

		vk::AttachmentReference depthAttachmentReference{
			.attachment = 1,
			.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
		};

		vk::SubpassDescription swapchainSubpassDescription{
			.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentReference,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = &depthAttachmentReference
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
				.attachmentCount = static_cast<uint32_t>(swapchainAttachmentDescriptions.size()),
				.pAttachments = swapchainAttachmentDescriptions.data(),
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
			std::array views = { *imageView, *uniqueDepthBufferImageView };
			auto [createFramebufferResult, uniqueFramebuffer] = uniqueDevice->createFramebufferUnique(
				vk::FramebufferCreateInfo{
					.renderPass = *uniqueRenderPass,
					.attachmentCount = 2,
					.pAttachments = views.data(),
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

		auto material = pScene->mMaterials[loweMeshes.front()->mMaterialIndex];
		aiString textureFilePath;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath) == aiReturn_SUCCESS) {
			std::cout << "Texture file path: " << textureFilePath.C_Str() << std::endl;
		}
		else {
			std::cerr << "Failed to get texture file path from material" << std::endl;
		}
		std::string loweTextureFilePath = "assets/models/lowe/" + std::string(textureFilePath.C_Str());
		Image loweTextureImage{loweTextureFilePath};

		auto [loweImageCreateResult, uniqueLoweAllocatedImage] = uniqueAllocator->createAndAllocateImageUnique(
			vk::ImageCreateInfo{
				.imageType = vk::ImageType::e2D,
				.format = vk::Format::eR8G8B8A8Srgb,
				.extent = vk::Extent3D{
					.width = loweTextureImage.getData().width,
					.height = loweTextureImage.getData().height,
					.depth = 1
				},
				.mipLevels = 14,
				.arrayLayers = 1,
				.samples = vk::SampleCountFlagBits::e1,
				.tiling = vk::ImageTiling::eOptimal,
				.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr,
				.initialLayout = vk::ImageLayout::eUndefined
			},
			vma::MemoryUsage::ePreferDeviceMemory
		);
		if (loweImageCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create image for lowe texture");
		}

		auto [createLoweImageViewResult, uniqueLoweImageView] = uniqueDevice->createImageViewUnique(
			vk::ImageViewCreateInfo{
				.image = *uniqueLoweAllocatedImage,
				.viewType = vk::ImageViewType::e2D,
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
					.levelCount = 14,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			});
		if (createLoweImageViewResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create image view for lowe texture");
		}

		vk::DescriptorSetLayoutBinding loweDescSetLayoutBindings[]{
			{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment
			},
			{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex
			}
		};

		std::array descriptorSetLayoutBindings{
			vk::DescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment
			},
			vk::DescriptorSetLayoutBinding{
				.binding = 0,
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
				.descriptorCount = 3
			},
			vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 3
			}
		};

		auto [descriptorPoolCreateResult, uniqueDescriptorPool] = uniqueDevice->createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo{
				.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				.maxSets = 3,
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
		};

		float fov = glm::radians(45.0f);        // Угол обзора 45 градусов
		float aspect = static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);  // Соотношение сторон окна
		float nearPlane = 0.1f;                // Ближняя плоскость (не ставь 0!)
		float farPlane = 1000.0f;                // Дальняя плоскость

		glm::mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);

		// КРИТИЧЕСКИ ВАЖНО ДЛЯ VULKAN:
		proj[1][1] *= -1;

		CameraData cameraData{
			.view = camera.getViewMatrix(),
			.projection = proj
		};

		auto [cameraBufferCreateResult, uniqueCameraBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(CameraData),
				.usage = vk::BufferUsageFlagBits::eUniformBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::ePreferHostMemory,
			vma::AllocationCreateFlags{ vma::AllocationCreateFlagBits::eMapped } | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite
		);
		if (cameraBufferCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create camera uniform buffer");
		}

		auto [mapCameraBufferResult, cameraBufferDataPtr] = uniqueAllocator->mapMemory(*uniqueCameraBuffer);
		if (mapCameraBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to map camera buffer memory");
		}
		std::memcpy(cameraBufferDataPtr, &cameraData, sizeof(CameraData));
		uniqueAllocator->unmapMemory(*uniqueCameraBuffer);

		CubeMapImageFiles files{
			.rightFilePath = "img/right.png",
			.leftFilePath = "img/left.png",
			.topFilePath = "img/up.png",
			.bottomFilePath = "img/down.png",
			.frontFilePath = "img/front.png",
			.backFilePath = "img/back.png"
		};

		CubeMapImage cubeMap{files};
		auto sideWidth = cubeMap.getData().sideWidth;

		auto [createCubeMapResult, uniqueCubeMapMemoryResource] = uniqueAllocator->createAndAllocateImageUnique(
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
		if (createCubeMapResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube map image");
		}

		auto [createUniqueAllocatedBufferResult, uniqueAllocatedStagingBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = loweTextureImage.getTotalSize() > cubeMap.getTotalDataSize() ? loweTextureImage.getTotalSize() : cubeMap.getTotalDataSize(),
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped
		);
		if (createUniqueAllocatedBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create staging buffer");
		}

		vcm::StagingBuffer stagingBuffer{
			std::move(uniqueAllocatedStagingBuffer),
			vma::AllocatorCopier{ *uniqueAllocator }
		};


		{
			auto [createCopySemaphoreFinishedResult, uniqueCopySemaphoreFinished] = uniqueDevice->createSemaphoreUnique({});
			auto [copyCompleteFenceCreateResult, uniqueCopyCompleteFence] = uniqueDevice->createFenceUnique({});
			auto [copyCubeMapFenceCreateResult, uniqueCopyCubeMapFence] = uniqueDevice->createFenceUnique({});
			if (createCopySemaphoreFinishedResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create copy finished semaphore");
			}
			if (copyCubeMapFenceCreateResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create copy cube map fence");
			}
			if (copyCompleteFenceCreateResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to create copy complete fence");
			}

			if (auto resultWrite = stagingBuffer.writeCubeMapData(
				transferQueue,
				cubeMapCopyCommandBuffer, // Command buffer should be created and passed here
				{}, // Wait semaphores should be created and passed here
				{*uniqueCopySemaphoreFinished}, // Signal semaphores should be created and passed here
				*uniqueCopyCubeMapFence,
				*uniqueCubeMapMemoryResource,
				cubeMap
			); resultWrite != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to write cube map data using staging buffer");
			}
			if (auto waitResult = uniqueDevice->waitForFences(*uniqueCopyCubeMapFence, VK_TRUE, UINT64_MAX); waitResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to wait for fences");
			}
			if (auto resetResult = uniqueDevice->resetFences(*uniqueCopyCubeMapFence); resetResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to reset fences");
			}
			if (auto resultWrite = stagingBuffer.writeTexture2dData(
				transferQueue,
				texture2dCopyCommandBuffer, // Command buffer should be created and passed here
				{*uniqueCopySemaphoreFinished}, // Wait semaphores should be created and passed here
				{}, // Signal semaphores should be created and passed here
				*uniqueCopyCompleteFence,
				*uniqueLoweAllocatedImage,
				loweTextureImage
			); resultWrite != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to write 2D texture data using staging buffer");
			}
			if( auto waitResult = uniqueDevice->waitForFences(*uniqueCopyCompleteFence, VK_TRUE, UINT64_MAX); waitResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to wait for fences");
			}
			if (auto resetResult = uniqueDevice->resetFences(*uniqueCopyCompleteFence); resetResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to reset fences");
			}

			if (auto result = stagingBuffer.generateMipmaps(
				transferQueue,
				cubeMapCopyCommandBuffer,
				{}, // Wait semaphores should be created and passed here
				{}, // Signal semaphores should be created and passed here
				*uniqueCopyCompleteFence,
				*uniqueLoweAllocatedImage,
				vk::Extent2D {
					loweTextureImage.getData().width,
					loweTextureImage.getData().height,
				},
				14
				); result != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to generate mipmaps");
			}
			if (auto waitResult = uniqueDevice->waitForFences(*uniqueCopyCompleteFence, VK_TRUE, UINT64_MAX); waitResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to wait for fences");
			}
		}

		auto [loweTextureSamplerCreateResult, uniqueLoweTextureSampler] = uniqueDevice->createSamplerUnique(
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
				.maxLod = 16.0f,
				.borderColor = vk::BorderColor::eIntOpaqueBlack,
				.unnormalizedCoordinates = vk::False
			}
		);
		if (loweTextureSamplerCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create sampler for lowe texture");
		}

		auto [createLoweDescriptorLayoutResult, uniqueLoweDescriptorSetLayout] = uniqueDevice->createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo{
				.bindingCount = static_cast<uint32_t>(std::size(loweDescSetLayoutBindings)),
				.pBindings = loweDescSetLayoutBindings
			}
		);
		if (createLoweDescriptorLayoutResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create descriptor set layout for lowe texture");
		}

		auto [allocateLoweDescriptorSetResult, uniqueLoweDescriptorSets] = uniqueDevice->allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo{
				.descriptorPool = *uniqueDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &uniqueLoweDescriptorSetLayout.get()
			}
		);
		if (allocateLoweDescriptorSetResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to allocate descriptor set for lowe texture");
		}
		auto& uniqueLoweDescriptorSet = uniqueLoweDescriptorSets.front();

		auto imageInfo = vk::DescriptorImageInfo{
			.sampler = *uniqueLoweTextureSampler,
			.imageView = *uniqueLoweImageView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};


		vk::DescriptorBufferInfo cameraBufferDescriptorInfo{
			.buffer = *uniqueCameraBuffer,
			.offset = 0,
			.range = sizeof(CameraData)
		};

		auto [loweMatricesBufferCreateResult, uniqueLoweMatricesBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(glm::mat4) * 3,
				.usage = vk::BufferUsageFlagBits::eUniformBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			}, vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eHostAccessSequentialWrite
			);
		if (loweMatricesBufferCreateResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create matrices buffer for lowe texture");
		}

		vk::DescriptorBufferInfo loweMatricesBufferDescriptorInfo{
			.buffer = *uniqueLoweMatricesBuffer,
			.offset = 0,
			.range = sizeof(glm::mat4) * 3
		};

		uniqueDevice->updateDescriptorSets(
			{vk::WriteDescriptorSet{
				.dstSet = *uniqueLoweDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &imageInfo
			},
			vk::WriteDescriptorSet{
				.dstSet = *uniqueLoweDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &loweMatricesBufferDescriptorInfo
			}},
			{}
		);

		auto [cubeMapImageViewCreateResult, uniqueCubeMapImageView] = uniqueDevice->createImageViewUnique(
			vk::ImageViewCreateInfo{
				.image = *uniqueCubeMapMemoryResource,
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

		std::array writeDescriptorSets{
			vk::WriteDescriptorSet{
				.dstSet = *uniqueDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &cubeMapDescriptorImageInfo
			},
			vk::WriteDescriptorSet{
				.dstSet = *uniqueDescriptorSet,
				.dstBinding = 0,
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

		auto [createAndAllocateLoweVertexBufferResult, uniqueLoweVertexBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = vertexBufferSize, // Assuming position only for simplicity
				.usage = vk::BufferUsageFlagBits::eVertexBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped
		);
		if (createAndAllocateLoweVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create lowe vertex buffer");
		}

		auto [createAndAllocateLoweIndexBufferResult, uniqueLoweIndexBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
			.size = indexBufferSize,
			.usage = vk::BufferUsageFlagBits::eIndexBuffer,
			.sharingMode = vk::SharingMode::eExclusive,
			},
			vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped);
		if (createAndAllocateLoweIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create lowe index buffer");
		}

		// write vertex data to vertex buffer
		std::vector<DrawCallInfo> loweDrawCallInfos;
		uint32_t firstIndex = 0;
		int32_t vertexOffset = 0;
		for (auto loweMesh : loweMeshes) {

			DrawCallInfo loweDrawCallInfo{
				.indexCount = loweMesh->mNumFaces * 3,
				.firstIndex = firstIndex,
				.vertexOffset = vertexOffset
			};

			loweDrawCallInfos.push_back(loweDrawCallInfo);

			for (int i = 0; i < loweMesh->mNumFaces; i++) {
				vma::BufferWriteInfo indexBufferWriteInfo{
					.dstBuffer = *uniqueLoweIndexBuffer,
					.dstBufferOffset = (firstIndex + i * 3) * 4,
					.srcData = loweMesh->mFaces[i].mIndices,
					.dataSize = 3 * sizeof(uint32_t)
				};
				if (auto writeResult = uniqueAllocator->writeBufferFromHost(indexBufferWriteInfo); writeResult != vk::Result::eSuccess) {
					throw std::runtime_error("Failed to create lowe index buffer");
				}
			}

			auto [mapVertexBufferResult, vertexBufferDataPtr] = uniqueAllocator->mapMemory(*uniqueLoweVertexBuffer);
			if (mapVertexBufferResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to map lowe vertex buffer memory");
			}

			uint32_t positionAttributeSize = sizeof(float) * 3;
			uint32_t texCoordAttributeSize = sizeof(float) * 2;

			uint32_t positionAttributeOffset = vertexOffset * positionAttributeSize;
			uint32_t texCoordAttributeOffset = vertexTexCoordAttributeInitialOffset + vertexOffset * texCoordAttributeSize;

			strideCopy(
				static_cast<char*>(vertexBufferDataPtr) + positionAttributeOffset,
				loweMesh->mVertices,
				sizeof(float) * 3,
				loweMesh->mNumVertices,
				sizeof(float) * 3,
				sizeof(float) * 3
			);

			strideCopy(
				static_cast<char*>(vertexBufferDataPtr) + texCoordAttributeOffset,
				loweMesh->mTextureCoords[0],
				sizeof(float) * 2,
				loweMesh->mNumVertices,
				sizeof(float) * 2,
				sizeof(float) * 3
				);

			uniqueAllocator->unmapMemory(*uniqueLoweVertexBuffer);

			firstIndex += loweMesh->mNumFaces * 3;
			vertexOffset += static_cast<int32_t>(loweMesh->mNumVertices);
		}

		auto [createLoweVertexShaderModuleResult, uniqueLoweVertexShaderModule] = loadAndCreateShaderModuleUnique(*uniqueDevice, vk::PipelineStageFlagBits::eVertexShader, "shaders/simpleModel.vert.spv");
		auto [createLoweFragmentShaderModuleResult, uniqueLoweFragmentShaderModule] = loadAndCreateShaderModuleUnique(*uniqueDevice, vk::PipelineStageFlagBits::eFragmentShader, "shaders/simpleModel.frag.spv");

		if (createLoweVertexShaderModuleResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create lowe vertex shader module");
		}
		if (createLoweFragmentShaderModuleResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create lowe fragment shader module");
		}

		vk::PipelineShaderStageCreateInfo loweShaderStagesCreateInfos[]{
			{
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *uniqueLoweVertexShaderModule,
				.pName = "main"
			},
			{
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *uniqueLoweFragmentShaderModule,
				.pName = "main"
			}
		};

		vk::VertexInputBindingDescription loweVertexBindingDescriptions[] {
			{
				.binding = 0,
				.stride = 3 * sizeof(float),
				.inputRate = vk::VertexInputRate::eVertex
			},
			{
				.binding = 1,
				.stride = 2 * sizeof(float),
				.inputRate = vk::VertexInputRate::eVertex
			}
		};

		vk::VertexInputAttributeDescription loweVertexAttributeDescriptions[] {
			{
				.location = 0,
				.binding = 0,
				.format = vk::Format::eR32G32B32Sfloat,
				.offset = 0,
			},
			{
				.location = 1,
				.binding = 1,
				.format = vk::Format::eR32G32Sfloat,
				.offset = 0
			}
		};

		vk::PipelineVertexInputStateCreateInfo loweVertexInputStateCreateInfo {
			.vertexBindingDescriptionCount = static_cast<uint32_t>(std::size(loweVertexBindingDescriptions)),
			.pVertexBindingDescriptions = loweVertexBindingDescriptions,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(std::size(loweVertexAttributeDescriptions)),
			.pVertexAttributeDescriptions = loweVertexAttributeDescriptions
		};

		vk::PipelineInputAssemblyStateCreateInfo loweInputAssemblyStateCreateInfo {
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};

		vk::Viewport loweViewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapchainExtent.width),
			.height = static_cast<float>(swapchainExtent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		vk::Rect2D loweScissor{
			.offset{
				.x = 0,
				.y = 0
			},
			.extent{
				.width = swapchainExtent.width,
				.height = swapchainExtent.height
			}
		};

		vk::PipelineViewportStateCreateInfo loweViewportStateCreateInfo {
			.viewportCount = 1,
			.pViewports = &loweViewport, // Dynamic state
			.scissorCount = 1,
			.pScissors = &loweScissor // Dynamic state
		};

		vk::PipelineRasterizationStateCreateInfo loweRasterizationStateCreateInfo {
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f
		};

		vk::PipelineMultisampleStateCreateInfo loweMultisampleStateCreateInfo {
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False
		};

		vk::PipelineDepthStencilStateCreateInfo loweDepthStencilStateCreateInfo {
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False
		};

		vk::PipelineColorBlendAttachmentState loweColorBlendAttachmentState {
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};

		vk::PipelineColorBlendStateCreateInfo loweColorBlendStateCreateInfo {
			.logicOpEnable = vk::False,
			.attachmentCount = 1,
			.pAttachments = &loweColorBlendAttachmentState
		};

		auto [createLowePipelineLayoutResult, uniqueLowePipelineLayout] = uniqueDevice->createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo{
				.setLayoutCount = 1,
				.pSetLayouts = &uniqueLoweDescriptorSetLayout.get()
			}
		);
		if (createLowePipelineLayoutResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create pipeline layout for lowe");
		}

		auto [createLoweGraphicsPipelineResult, uniqueLoweGraphicsPipeline] = uniqueDevice->createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo{
				.stageCount = 2,
				.pStages = loweShaderStagesCreateInfos,
				.pVertexInputState = &loweVertexInputStateCreateInfo,
				.pInputAssemblyState = &loweInputAssemblyStateCreateInfo,
				.pViewportState = &loweViewportStateCreateInfo,
				.pRasterizationState = &loweRasterizationStateCreateInfo,
				.pMultisampleState = &loweMultisampleStateCreateInfo,
				.pDepthStencilState = &loweDepthStencilStateCreateInfo,
				.pColorBlendState = &loweColorBlendStateCreateInfo,
				.pDynamicState = nullptr,
				.layout = *uniqueLowePipelineLayout,
				.renderPass = *uniqueRenderPass,
				.subpass = 0
			});
		if (createLoweGraphicsPipelineResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create graphics pipeline for lowe");
		}

		struct CubeVertex {
			glm::vec3 position;
			glm::vec3 directionVector;
		};

		// Вершины (центрированы от -0.1 до 0.1)
		std::array cubeVertices{
		  CubeVertex{.position{-0.1f, -0.1f, -0.1f}, .directionVector{-1.0f, -1.0f, -1.0f} }, // 0
		  CubeVertex{.position{ 0.1f, -0.1f, -0.1f}, .directionVector{ 1.0f, -1.0f, -1.0f} }, // 1
		  CubeVertex{.position{ 0.1f,  0.1f, -0.1f}, .directionVector{ 1.0f,  1.0f, -1.0f} }, // 2
		  CubeVertex{.position{-0.1f,  0.1f, -0.1f}, .directionVector{-1.0f,  1.0f, -1.0f} }, // 3
		  CubeVertex{.position{-0.1f, -0.1f,  0.1f}, .directionVector{-1.0f, -1.0f,  1.0f} }, // 4
		  CubeVertex{.position{ 0.1f, -0.1f,  0.1f}, .directionVector{ 1.0f, -1.0f,  1.0f} }, // 5
		  CubeVertex{.position{ 0.1f,  0.1f,  0.1f}, .directionVector{ 1.0f,  1.0f,  1.0f} }, // 6
		  CubeVertex{.position{-0.1f,  0.1f,  0.1f}, .directionVector{-1.0f,  1.0f,  1.0f} }  // 7
		};

		// Индексы (все CCW при взгляде СНАРУЖИ)
		std::array cubeIndices{
			// Front face (Z = 0.1)
			4, 5, 6, 6, 7, 4,
			// Back face (Z = -0.1)
			1, 0, 3, 3, 2, 1,
			// Left face (X = -0.1)
			4, 7, 3, 3, 0, 4,
			// Right face (X = 0.1)
			1, 2, 6, 6, 5, 1,
			// Top face (Y = 0.1)
			6, 2, 3, 3, 7, 6,
			// Bottom face (Y = -0.1)
			4, 0, 1, 1, 5, 4
		};



		struct ModelData {
			glm::mat4 model;
		};

		ModelData cubePosition{
			.model{glm::mat4(1.0f)}
		};

		auto [createCubeVertexBufferResult, uniqueCubeVertexBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
			.size = sizeof(cubeVertices),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = vk::SharingMode::eExclusive
			}, vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped);
		if (createCubeVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube vertex buffer");
		}

		auto [createCubeIndexBufferResult, uniqueCubeIndexBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
			.size = sizeof(cubeIndices),
			.usage = vk::BufferUsageFlagBits::eIndexBuffer,
			.sharingMode = vk::SharingMode::eExclusive
			}, vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped);
		if (createCubeIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube index buffer");
		}

		vma::BufferWriteInfo cubeVertexBufferMemoryRange{
			.dstBuffer = *uniqueCubeVertexBuffer,
			.dstBufferOffset = 0,
			.srcData = cubeVertices.data(),
			.dataSize = sizeof(cubeVertices)
		};

		vma::BufferWriteInfo cubeIndexBufferMemoryRange{
			.dstBuffer = *uniqueCubeIndexBuffer,
			.dstBufferOffset = 0,
			.srcData = cubeIndices.data(),
			.dataSize = sizeof(cubeIndices)
		};

		if (auto writeCubeVertexBufferResult = uniqueAllocator->writeBufferFromHost(cubeVertexBufferMemoryRange); writeCubeVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to write cube vertex buffer data");
		}

		if (auto writeCubeIndexBufferResult = uniqueAllocator->writeBufferFromHost(cubeIndexBufferMemoryRange); writeCubeIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to write cube index buffer data");
		}

		auto [createCubePipelineLayoutResult, uniqueCubePipelineLayout] = uniqueDevice->createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo{
				.setLayoutCount = 1,
				.pSetLayouts = &uniqueDescriptorSetLayout.get()
			}
		);
		if (createCubePipelineLayoutResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create cube pipeline layout");
		}

		auto [loadSkyboxVertexShaderModuleResult, uniqueSkyboxVertexShaderModule] =
			loadAndCreateShaderModuleUnique(*uniqueDevice, vk::PipelineStageFlagBits::eVertexShader, "shaders/skybox.vert.spv");

		auto [loadSkyboxFragmentShaderModuleResult, uniqueSkyboxFragmentShaderModule] =
			loadAndCreateShaderModuleUnique(*uniqueDevice, vk::PipelineStageFlagBits::eFragmentShader, "shaders/skybox.frag.spv");

		vk::PipelineShaderStageCreateInfo skyboxShaderStages[2]{
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *uniqueSkyboxVertexShaderModule,
				.pName = "main"
			},
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *uniqueSkyboxFragmentShaderModule,
				.pName = "main"
			}
		};

		// Pipeline for skybox should be created here in a similar way, with different shaders and possibly different pipeline states (e.g. cull mode, depth test/write)
		std::array skyboxVertices{
			glm::vec3{-1.0f, -1.0f, -1.0f},
			glm::vec3{ 1.0f, -1.0f, -1.0f},
			glm::vec3{ 1.0f,  1.0f, -1.0f},
			glm::vec3{-1.0f,  1.0f, -1.0f},
			glm::vec3{-1.0f, -1.0f,  1.0f},
			glm::vec3{ 1.0f, -1.0f,  1.0f},
			glm::vec3{ 1.0f,  1.0f,  1.0f},
			glm::vec3{-1.0f,  1.0f,  1.0f}
		};

		uint32_t skyboxIndices[] = {
			// Передняя грань (Z = -1)
			0, 3, 2,
			2, 1, 0,
			// Задняя грань (Z = 1)
			4, 5, 6,
			6, 7, 4,
			// Левая грань (X = -1)
			0, 4, 7,
			7, 3, 0,
			// Правая грань (X = 1)
			1, 2, 6,
			6, 5, 1,
			// Верхняя грань (Y = 1)
			2, 3, 7,
			7, 6, 2,
			// Нижняя грань (Y = -1)
			0, 1, 5,
			5, 4, 0
		};

		auto [createSkyboxVertexBufferResult, uniqueSkyboxVertexBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(skyboxVertices),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped
		);
		if (createSkyboxVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create skybox vertex buffer");
		}

		if (auto writeSkyboxVertexBufferResult = uniqueAllocator->writeBufferFromHost(
			vma::BufferWriteInfo{
				.dstBuffer = *uniqueSkyboxVertexBuffer,
				.dstBufferOffset = 0,
				.srcData = skyboxVertices.data(),
				.dataSize = sizeof(skyboxVertices)
			}
		); writeSkyboxVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to write skybox vertex buffer data");
		}

		auto [createSkyboxIndexBufferResult, uniqueSkyboxIndexBuffer] = uniqueAllocator->createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(skyboxIndices),
				.usage = vk::BufferUsageFlagBits::eIndexBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::eCpuToGpu,
			vma::AllocationCreateFlagBits::eMapped
		);
		if (createSkyboxIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create skybox index buffer");
		}

		if (auto writeSkyboxIndexBufferResult = uniqueAllocator->writeBufferFromHost(
			vma::BufferWriteInfo{
				.dstBuffer = *uniqueSkyboxIndexBuffer,
				.dstBufferOffset = 0,
				.srcData = skyboxIndices,
				.dataSize = sizeof(skyboxIndices)
			}
		); writeSkyboxIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to write skybox index buffer data");
		}

		vk::VertexInputAttributeDescription skyboxVertexInputAttribute{
			.location = 0,
			.binding = 0,
			.format = vk::Format::eR32G32B32Sfloat,
			.offset = 0
		};

		vk::VertexInputBindingDescription skyboxVertexInputBinding{
			.binding = 0,
			.stride = sizeof(glm::vec3),
			.inputRate = vk::VertexInputRate::eVertex
		};

		vk::PipelineVertexInputStateCreateInfo skyboxVertexInputStateCreateInfo{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &skyboxVertexInputBinding,
			.vertexAttributeDescriptionCount = 1,
			.pVertexAttributeDescriptions = &skyboxVertexInputAttribute
		};

		vk::PipelineInputAssemblyStateCreateInfo skyboxInputAssemblyStateCreateInfo{
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};

		vk::Viewport skyboxViewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapchainExtent.width),
			.height = static_cast<float>(swapchainExtent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		vk::Rect2D skyboxScissor{
			.offset = vk::Offset2D{0, 0},
			.extent = swapchainExtent
		};

		vk::PipelineViewportStateCreateInfo skyboxViewPortStateCreateInfo{
			.viewportCount = 1,
			.pViewports = &skyboxViewport,
			.scissorCount = 1,
			.pScissors = &skyboxScissor
		};

		vk::PipelineRasterizationStateCreateInfo skyboxRasterizationStateCreateInfo{
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eNone,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f
		};

		vk::PipelineMultisampleStateCreateInfo skyboxMultisampleStateCreateInfo{
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False
		};

		vk::PipelineDepthStencilStateCreateInfo skyboxDepthStencilStateCreateInfo{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::False, // Don't write to depth buffer for skybox
			.depthCompareOp = vk::CompareOp::eLessOrEqual, // Use less or equal to ensure skybox is rendered behind all other geometry
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False
		};

		vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};

		vk::PipelineColorBlendStateCreateInfo skyboxColorBlendStateCreateInfo{
			.logicOpEnable = vk::False,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachmentState
		};

		auto [uniqueSkyboxGraphicsPipelineResult, uniqueSkyboxGraphicsPipeline] = uniqueDevice->createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo{
				.stageCount = 2,
				.pStages = skyboxShaderStages,
				.pVertexInputState = &skyboxVertexInputStateCreateInfo,
				.pInputAssemblyState = &skyboxInputAssemblyStateCreateInfo,
				.pViewportState = &skyboxViewPortStateCreateInfo,
				.pRasterizationState = &skyboxRasterizationStateCreateInfo,
				.pMultisampleState = &skyboxMultisampleStateCreateInfo,
				.pDepthStencilState = &skyboxDepthStencilStateCreateInfo,
				.pColorBlendState = &skyboxColorBlendStateCreateInfo,
				.layout = *uniqueCubePipelineLayout, // Assuming same pipeline layout can be used for skybox
				.renderPass = *uniqueRenderPass,
				.subpass = 0
			}
		);
		if (uniqueSkyboxGraphicsPipelineResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create skybox graphics pipeline.");
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

		std::array clearValues{
			vk::ClearValue{
				.color = vk::ClearColorValue{ std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f} }
			},
			vk::ClearValue{
				.depthStencil = vk::ClearDepthStencilValue{ .depth = 1.0f, .stencil = 0 }
			}
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
					.clearValueCount = 2,
					.pClearValues = clearValues.data()
				},
				vk::SubpassContents::eInline
			);
			// lowe rendering
			commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *uniqueLoweGraphicsPipeline);
			commandBuffer->bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				*uniqueLowePipelineLayout,
				0,
				{ *uniqueLoweDescriptorSet },
				{}
			);
			commandBuffer->bindVertexBuffers(0, { *uniqueLoweVertexBuffer, *uniqueLoweVertexBuffer }, { 0, vertexTexCoordAttributeInitialOffset });
			commandBuffer->bindIndexBuffer(*uniqueLoweIndexBuffer, 0, vk::IndexType::eUint32);
			for (auto [indexCount, firstIndex_, vertexOffset_]: loweDrawCallInfos) {
				commandBuffer->drawIndexed(indexCount, 1, firstIndex_, vertexOffset_, 0);
			}
			// render skybox
			commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *uniqueSkyboxGraphicsPipeline);
			commandBuffer->bindVertexBuffers(0, { *uniqueSkyboxVertexBuffer }, { 0 });
			commandBuffer->bindIndexBuffer(*uniqueSkyboxIndexBuffer, 0, vk::IndexType::eUint32);
			commandBuffer->bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				*uniqueCubePipelineLayout, // Assuming same pipeline layout is used for skybox
				0,
				{ *uniqueDescriptorSet },
				{}
			);
			commandBuffer->drawIndexed(std::size(skyboxIndices), 1, 0, 0, 0);
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

		enum class CameraMoveDirectionFlagBits {
			Forward = 0x1,
			Backward = 0x2,
			Left = 0x4,
			Right = 0x8,
			Up = 0x10,
			Down = 0x20
		};

		enum class CameraRotationDirectionFlagBits {
			PitchUp = 0x1,
			PitchDown = 0x2,
			YawLeft = 0x4,
			YawRight = 0x8,
			RollClock = 0x10,
			RollCounterClock = 0x20
		};

		std::underlying_type_t<CameraMoveDirectionFlagBits> cameraMoveDirectionFlags = 0;
		std::underlying_type_t<CameraRotationDirectionFlagBits> cameraRotationDirectionFlags = 0;


		window.setKeyboardEventCallback([&](SdlWindow&, SdlKeyCode keyCode, SdlKeyMode keyMode, SdlKeyState keyState) {
			if (keyCode == SdlKeyCode::Escape && keyState == SdlKeyState::Pressed) {
				std::cout << "Escape key pressed, closing window...\n";
				sdlLibrary.postQuitEvent();
			}

			static bool isRelativeMouseMode = false;

			if (keyState == SdlKeyState::Pressed) {

				switch (keyCode) {
					case SdlKeyCode::W:
						cameraMoveDirectionFlags |= static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Forward);
						break;
					case SdlKeyCode::S:
						cameraMoveDirectionFlags |= static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Backward);
						break;
					case SdlKeyCode::A:
						cameraMoveDirectionFlags |= static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Left);
						break;
					case SdlKeyCode::D:
						cameraMoveDirectionFlags |= static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Right);
						break;
					case SdlKeyCode::Q:
						cameraMoveDirectionFlags |= static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Up);
						break;
					case SdlKeyCode::E:
						cameraMoveDirectionFlags |= static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Down);
						break;
					case SdlKeyCode::Up:
						cameraRotationDirectionFlags |= static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::PitchUp);
						break;
					case SdlKeyCode::Down:
						cameraRotationDirectionFlags |= static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::PitchDown);
						break;
					case SdlKeyCode::Left:
						cameraRotationDirectionFlags |= static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::YawLeft);
						break;
					case SdlKeyCode::Right:
						cameraRotationDirectionFlags |= static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::YawRight);
						break;
					case SdlKeyCode::PageUp:
						cameraRotationDirectionFlags |= static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::RollCounterClock);
						break;
					case SdlKeyCode::PageDown:
						cameraRotationDirectionFlags |= static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::RollClock);
						break;
					case SdlKeyCode::Space:
						if (isRelativeMouseMode) {
							sdlLibrary.setRelativeMouseMode(false);
							isRelativeMouseMode = false;
						}
						else {
							sdlLibrary.setRelativeMouseMode(true);
							isRelativeMouseMode = true;
						}
						break;
					default:
						break;
				}
			}
			else if (keyState == SdlKeyState::Released) {
				switch (keyCode) {
					case SdlKeyCode::W:
						cameraMoveDirectionFlags &= ~static_cast<std::underlying_type<CameraMoveDirectionFlagBits>::type>(CameraMoveDirectionFlagBits::Forward);
						break;
					case SdlKeyCode::S:
						cameraMoveDirectionFlags &= ~static_cast<std::underlying_type<CameraMoveDirectionFlagBits>::type>(CameraMoveDirectionFlagBits::Backward);
						break;
					case SdlKeyCode::A:
						cameraMoveDirectionFlags &= ~static_cast<std::underlying_type<CameraMoveDirectionFlagBits>::type>(CameraMoveDirectionFlagBits::Left);
						break;
					case SdlKeyCode::D:
						cameraMoveDirectionFlags &= ~static_cast<std::underlying_type<CameraMoveDirectionFlagBits>::type>(CameraMoveDirectionFlagBits::Right);
						break;
					case SdlKeyCode::Q:
						cameraMoveDirectionFlags &= ~static_cast<std::underlying_type<CameraMoveDirectionFlagBits>::type>(CameraMoveDirectionFlagBits::Up);
						break;
					case SdlKeyCode::E:
						cameraMoveDirectionFlags &= ~static_cast<std::underlying_type<CameraMoveDirectionFlagBits>::type>(CameraMoveDirectionFlagBits::Down);
						break;
					case SdlKeyCode::Up:
						cameraRotationDirectionFlags &= ~static_cast<std::underlying_type<CameraRotationDirectionFlagBits>::type>(CameraRotationDirectionFlagBits::PitchUp);
						break;
					case SdlKeyCode::Down:
						cameraRotationDirectionFlags &= ~static_cast<std::underlying_type<CameraRotationDirectionFlagBits>::type>(CameraRotationDirectionFlagBits::PitchDown);
						break;
					case SdlKeyCode::Left:
						cameraRotationDirectionFlags &= ~static_cast<std::underlying_type<CameraRotationDirectionFlagBits>::type>(CameraRotationDirectionFlagBits::YawLeft);
						break;
					case SdlKeyCode::Right:
						cameraRotationDirectionFlags &= ~static_cast<std::underlying_type<CameraRotationDirectionFlagBits>::type>(CameraRotationDirectionFlagBits::YawRight);
						break;
					case SdlKeyCode::PageUp:
						cameraRotationDirectionFlags &= ~static_cast<std::underlying_type<CameraRotationDirectionFlagBits>::type>(CameraRotationDirectionFlagBits::RollCounterClock);
						break;
					case SdlKeyCode::PageDown:
						cameraRotationDirectionFlags &= ~static_cast<std::underlying_type<CameraRotationDirectionFlagBits>::type>(CameraRotationDirectionFlagBits::RollClock);
						break;
					default:
						break;
				}
			}
			cameraData.view = camera.getViewMatrix();
		});


		while (running){

			if(!sdlLibrary.pullEvents()) break;
			float yawAngle = 0.0f;
			float pitchAngle = 0.0f;
			float rollAngle = 0.0f;
			if (cameraRotationDirectionFlags & static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::YawLeft)) {
				yawAngle += 0.01f;
			}
			if (cameraRotationDirectionFlags & static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::YawRight)) {
				yawAngle -= 0.01f;
			}
			if (cameraRotationDirectionFlags & static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::PitchUp)) {
				pitchAngle += 0.01f;
			}
			if (cameraRotationDirectionFlags & static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::PitchDown)) {
				pitchAngle -= 0.01f;
			}
			if (cameraRotationDirectionFlags & static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::RollClock)) {
				rollAngle += 0.01f;
			}
			if (cameraRotationDirectionFlags & static_cast<std::underlying_type_t<CameraRotationDirectionFlagBits>>(CameraRotationDirectionFlagBits::RollCounterClock)) {
				rollAngle -= 0.01f;
			}
			glm::quat rotationQuaternion = glm::quat(glm::vec3(pitchAngle, yawAngle, rollAngle));
			float angle = glm::angle(rotationQuaternion);
			glm::vec3 axis = glm::axis(rotationQuaternion);

			glm::vec3 movementVector = glm::vec3(0.0f, 0.0f, 0.0f);
			if (cameraMoveDirectionFlags & static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Forward)) {
				movementVector.z -= 0.01f;
			}
			if (cameraMoveDirectionFlags & static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Backward)) {
				movementVector.z += 0.01f;
			}
			if (cameraMoveDirectionFlags & static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Left)) {
				movementVector.x -= 0.01f;
			}
			if (cameraMoveDirectionFlags & static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Right)) {
				movementVector.x += 0.01f;
			}
			if (cameraMoveDirectionFlags & static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Up)) {
				movementVector.y += 0.01f;
			}
			if (cameraMoveDirectionFlags & static_cast<std::underlying_type_t<CameraMoveDirectionFlagBits>>(CameraMoveDirectionFlagBits::Down)) {
				movementVector.y -= 0.01f;
			}
			camera.rotate(angle, axis);
			camera.move(movementVector);
			cameraData.view = camera.getViewMatrix();


			vma::BufferWriteInfo updatedCameraData{
				.dstBuffer = *uniqueCameraBuffer,
				.dstBufferOffset = 0,
				.srcData = &cameraData,
				.dataSize = sizeof(CameraData)
			};

			if (auto result = uniqueAllocator->writeBufferFromHost(updatedCameraData); result != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to update camera uniform buffer");
			}

			glm::mat4 loweMatrices[3] {
				modelMatrix,
				cameraData.view,
				cameraData.projection
			};

			vma::BufferWriteInfo updatedModelData{
				.dstBuffer = *uniqueLoweMatricesBuffer,
				.dstBufferOffset = 0,
				.srcData = loweMatrices,
				.dataSize = sizeof(loweMatrices)
			};

			if (auto result = uniqueAllocator->writeBufferFromHost(updatedModelData); result != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to update model matrices buffer");
			}


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
					.pSignalSemaphores = &renderFinishedSemaphores[currentImageIndex].get()
				} }, inFlightFences[currentFrame].get());
			if (submitResult != vk::Result::eSuccess) {
				throw std::runtime_error("Failed to submit draw command buffer");
			}
			auto presentResult = presentationQueue.presentKHR(
				vk::PresentInfoKHR{
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &renderFinishedSemaphores[currentImageIndex].get(),
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