#pragma once
#include <utility>

#include "IncludeVulkan.hpp"
#include "ImageLoader/Image.hpp"
#include "VulkanDeviceAllocator/VulkanDeviceAllocator.hpp"

namespace vcm {
	class StagingBuffer {
	public:

		StagingBuffer(
			vma::UniqueAllocatedBuffer&& stagingBuffer,
			vma::AllocatorCopier memoryToBufferCopier
		) : stagingBuffer{ std::move(stagingBuffer) }, memoryToBufferCopier{std::move( memoryToBufferCopier )} {}

		[[nodiscard]] vk::Result writeCubeMapData(
			vk::Queue queue,
			vk::CommandBuffer commandBuffer,
			std::vector<vk::Semaphore> waitSemaphores,
			std::vector<vk::Semaphore> signalSemaphores,
			vk::Fence fence,
			vk::Image cubeMap, CubeMapImage const& cubeMapImageData
		) const;

		[[nodiscard]] vk::Result writeTexture2dData(
			vk::Queue queue,
			vk::CommandBuffer commandBuffer,
			std::vector<vk::Semaphore> waitSemaphores,
			std::vector<vk::Semaphore> signalSemaphores,
			vk::Fence fence,
			vk::Image texture2d,
			Image const& texture2dImageData) const;

		[[nodiscard]] vk::Result generateMipmaps(
			vk::Queue queue,
			vk::CommandBuffer commandBuffer,
			std::vector<vk::Semaphore> waitSemaphores,
			std::vector<vk::Semaphore> signalSemaphores,
			vk::Fence fence,
			vk::Image texture2d,
			vk::Extent2D imageExtent,
			uint32_t mipmapLevels);

	private:
		vma::AllocatorCopier memoryToBufferCopier;
		vma::UniqueAllocatedBuffer stagingBuffer;
	};
}