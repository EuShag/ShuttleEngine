#pragma once
#include "IncludeVulkan.hpp"
#include "../VulkanDeviceAllocator/VulkanDeviceAllocator.hpp"
#include "../ImageLoader/Image.hpp"
#include <array>
#include <glm/glm.hpp>

namespace vrender {
	class Skybox {
	public:
		Skybox(vk::Device device, vma::Allocator allocator, CubeMapImage cubeMap);

		void writeCommands(vk::CommandBuffer commandBuffer, vk::DescriptorSet cameraDescriptorSet) const;

		~Skybox();
	private:
		static constexpr glm::vec3 skyboxVertices[] {
			{-1.0f, -1.0f, -1.0f},
			{ 1.0f, -1.0f, -1.0f},
			{ 1.0f,  1.0f, -1.0f},
			{-1.0f,  1.0f, -1.0f},
			{-1.0f, -1.0f,  1.0f},
			{ 1.0f, -1.0f,  1.0f},
			{ 1.0f,  1.0f,  1.0f},
			{-1.0f,  1.0f,  1.0f}
		};
		static constexpr uint32_t skyboxVertexCount = std::size(skyboxVertices);

		static constexpr uint32_t skyboxIndices[] {
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
		static constexpr uint32_t skyboxIndexCount = std::size(skyboxIndices);

		vma::UniqueAllocatedBuffer vertexBuffer;
		vma::UniqueAllocatedBuffer indexBuffer;
		vma::UniqueAllocatedImage cubeImage;

		vk::UniquePipeline pipeline;
		vk::UniquePipelineLayout pipelineLayout;
		vk::UniqueDescriptorSet descriptorSet;
		
		vk::ImageView cubemapImageView;
		vk::Sampler cubemapSampler;
	};
}