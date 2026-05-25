#include "Skybox.hpp"

#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace vrender {

	Skybox::Skybox(vk::Device device, vma::Allocator allocator, CubeMapImage cubeMap, vk::RenderPass renderPass, vk::Extent2D extent, char* vertexShaderFile, char* fragmentShaderFile) {
		auto [loadSkyboxVertexShaderModuleResult, uniqueSkyboxVertexShaderModule] =
			loadAndCreateShaderModuleUnique(device, vk::PipelineStageFlagBits::eVertexShader, "shaders/skybox.vert.spv");

		auto [loadSkyboxFragmentShaderModuleResult, uniqueSkyboxFragmentShaderModule] =
			loadAndCreateShaderModuleUnique(device, vk::PipelineStageFlagBits::eFragmentShader, "shaders/skybox.frag.spv");

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

		auto [createSkyboxVertexBufferResult, uniqueSkyboxVertexBuffer] = allocator.createAndAllocateBufferUnique(
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

		if (auto writeSkyboxVertexBufferResult = allocator.writeBufferFromHost(
			vma::BufferWriteInfo{
				.dstBuffer = *uniqueSkyboxVertexBuffer,
				.dstBufferOffset = 0,
				.srcData = skyboxVertices,
				.dataSize = sizeof(skyboxVertices)
			}
		); writeSkyboxVertexBufferResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to write skybox vertex buffer data");
		}

		auto [createSkyboxIndexBufferResult, uniqueSkyboxIndexBuffer] = allocator.createAndAllocateBufferUnique(
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

		if (auto writeSkyboxIndexBufferResult = allocator.writeBufferFromHost(
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
			.width = static_cast<float>(extent.width),
			.height = static_cast<float>(extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		vk::Rect2D skyboxScissor{
			.offset = vk::Offset2D{0, 0},
			.extent = extent
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

		vk::PipelineColorBlendAttachmentState skyboxBlendAttachmentState{
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR |
							  vk::ColorComponentFlagBits::eG |
							  vk::ColorComponentFlagBits::eB |
							  vk::ColorComponentFlagBits::eA
		};

		vk::PipelineColorBlendStateCreateInfo skyboxColorBlendStateCreateInfo{
			.logicOpEnable = vk::False,
			.attachmentCount = 1,
			.pAttachments = &skyboxBlendAttachmentState
		};

		auto [uniqueSkyboxGraphicsPipelineResult, uniqueSkyboxGraphicsPipeline] = device.createGraphicsPipelineUnique(
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
				.layout = *pipelineLayout, // Assuming same pipeline layout can be used for skybox
				.renderPass = renderPass,
				.subpass = 0
			}
		);
		if (uniqueSkyboxGraphicsPipelineResult != vk::Result::eSuccess) {
			throw std::runtime_error("Failed to create skybox graphics pipeline.");
		}
	}

	void Skybox::writeDrawCommands(vk::CommandBuffer commandBuffer, vk::DescriptorSet cameraDescriptors) const {
		vk::DescriptorSet descriptorSets[] { cameraDescriptors, *descriptorSet };

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, {});
		vk::DeviceSize vertexBufferOffset = 0;
		commandBuffer.bindVertexBuffers(0, { *vertexBuffer }, vertexBufferOffset);
		commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);
		commandBuffer.drawIndexed(skyboxIndexCount, 1, 0, 0, 0);
	}
}
