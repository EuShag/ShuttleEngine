#pragma once
#include "IncludeVulkan.hpp"
#include "ImageLoader/Image.hpp"
#include "VulkanDeviceAllocator/VulkanDeviceAllocator.hpp"

namespace vcm {
	class StagingBuffer {
	public:

		StagingBuffer(
			vma::UniqueAllocatedBuffer&& stagingBuffer,
			vma::AllocatorCopier memoryToBufferCopier
		) : stagingBuffer{ std::move(stagingBuffer) }, memoryToBufferCopier{ memoryToBufferCopier } {}

		vk::Result writeCubeMapData(
			vk::Queue queue,
			vk::CommandBuffer commandBuffer,
			std::vector<vk::Semaphore> waitSemaphores,
			std::vector<vk::Semaphore> signalSemaphores,
			vk::Image cubeMap, CubeMapImage const& cubeMapImageData
		) const;

	private:
		vma::AllocatorCopier memoryToBufferCopier;
		vma::UniqueAllocatedBuffer stagingBuffer;
	};
}