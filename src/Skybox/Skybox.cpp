#include "Skybox.hpp"

namespace vrender {

	Skybox::Skybox(vk::Device device, vma::Allocator allocator, CubeMapImage cubeMap) {
		auto [createVertexBufferResult, uniqueVertexBuffer] = allocator.createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(skyboxVertices),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::eAuto,
			vma::AllocationCreateFlagBits::eHostAccessSequentialWrite
		);
		if (createVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create vertex buffer for skybox");
		} vertexBuffer = std::move(uniqueVertexBuffer);

		auto [createIndexBufferResult, uniqueIndexBuffer] = allocator.createAndAllocateBufferUnique(
			vk::BufferCreateInfo{
				.size = sizeof(skyboxIndices),
				.usage = vk::BufferUsageFlagBits::eIndexBuffer,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vma::MemoryUsage::eAuto,
			vma::AllocationCreateFlagBits::eHostAccessSequentialWrite
		);
		if (createIndexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create index buffer for skybox");
		} indexBuffer = std::move(uniqueIndexBuffer);
	}

	void Skybox::writeCommands(vk::CommandBuffer commandBuffer, vk::DescriptorSet cameraDescriptors) const {
		vk::DescriptorSet descriptorSets[] { cameraDescriptors, descriptorSet.get() };

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout.get(), 0, descriptorSets, {});
		vk::DeviceSize vertexBufferOffset = 0;
		commandBuffer.bindVertexBuffers(0, { *vertexBuffer }, vertexBufferOffset);
		commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);
		commandBuffer.drawIndexed(skyboxIndexCount, 1, 0, 0, 0);
	}
}