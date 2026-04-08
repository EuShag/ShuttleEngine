#include "VulkanCopyManager.hpp"

vk::Result vcm::StagingBuffer::writeCubeMapData(
	vk::Queue queue, 
	vk::CommandBuffer commandBuffer, 
	std::vector<vk::Semaphore> waitSemaphores,
	std::vector<vk::Semaphore> signalSemaphores,
	vk::Fence fence,
	vk::Image cubeMap, 
	CubeMapImage const& image) const
{
	uint32_t sideWidth = image.getData().sideWidth;
	size_t imageSideDataSize = image.getData().sideWidth * image.getData().sideWidth * 4;

	std::array writeToBufferInfo{
		vma::BufferWriteInfo{
			.dstBuffer = stagingBuffer.get(),
			.dstBufferOffset = 0,
			.srcData = image.getData().rightData,
			.dataSize = imageSideDataSize
		},
		vma::BufferWriteInfo{
			.dstBuffer = stagingBuffer.get(),
			.dstBufferOffset = imageSideDataSize * 1,
			.srcData = image.getData().leftData,
			.dataSize = imageSideDataSize
		},
		vma::BufferWriteInfo{
			.dstBuffer = stagingBuffer.get(),
			.dstBufferOffset = imageSideDataSize * 2,
			.srcData = image.getData().topData,
			.dataSize = imageSideDataSize
		},
		vma::BufferWriteInfo{
			.dstBuffer = stagingBuffer.get(),
			.dstBufferOffset = imageSideDataSize * 3,
			.srcData = image.getData().bottomData,
			.dataSize = imageSideDataSize
		},
		vma::BufferWriteInfo{
			.dstBuffer = stagingBuffer.get(),
			.dstBufferOffset = imageSideDataSize * 5,
			.srcData = image.getData().frontData,
			.dataSize = imageSideDataSize
		},
		vma::BufferWriteInfo{
			.dstBuffer = stagingBuffer.get(),
			.dstBufferOffset = imageSideDataSize * 4,
			.srcData = image.getData().backData,
			.dataSize = imageSideDataSize
		}
	};

	for (auto const& writeInfo : writeToBufferInfo) {
		if (auto result = memoryToBufferCopier.writeBufferFromHost(writeInfo); result != vk::Result::eSuccess) return result;
	}

	std::vector<vk::BufferImageCopy> bufferImageCopies(6);
	for (size_t i = 0; i < bufferImageCopies.size(); ++i) {
		auto bufferOffset = imageSideDataSize * i;
		bufferImageCopies[i] = vk::BufferImageCopy{
			.bufferOffset = bufferOffset,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = vk::ImageSubresourceLayers{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = static_cast<uint32_t>(i),
				.layerCount = 1
			},
			.imageOffset = vk::Offset3D{ .x = 0, .y = 0, .z = 0 },
			.imageExtent = vk::Extent3D{ .width = sideWidth, .height = sideWidth, .depth = 1 }
		};
	}

	if (auto result = commandBuffer.reset(vk::CommandBufferResetFlags{}); result != vk::Result::eSuccess) return result;
	if (auto result = commandBuffer.begin(vk::CommandBufferBeginInfo{}); result != vk::Result::eSuccess) return result;
	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {},
		{},
		{},
		vk::ImageMemoryBarrier{
			.srcAccessMask = {} ,
			.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = cubeMap,
			.subresourceRange = vk::ImageSubresourceRange{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 6
			}
		}
	);
	commandBuffer.copyBufferToImage(stagingBuffer.get(), cubeMap, vk::ImageLayout::eTransferDstOptimal, bufferImageCopies);
	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
		{},
		{},
		vk::ImageMemoryBarrier{
			.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
			.dstAccessMask = vk::AccessFlagBits::eShaderRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = cubeMap,
			.subresourceRange = vk::ImageSubresourceRange{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 6
			}
		}
	);
	if (auto result = commandBuffer.end(); result != vk::Result::eSuccess) return result;

	std::vector<vk::PipelineStageFlags> submitPipelineStages{ waitSemaphores.size(), vk::PipelineStageFlagBits::eTopOfPipe };

	if (auto result = queue.submit(
		vk::SubmitInfo{
			.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
			.pWaitSemaphores = waitSemaphores.data(),
			.pWaitDstStageMask = submitPipelineStages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
			.pSignalSemaphores = signalSemaphores.data()
		}, fence); result != vk::Result::eSuccess) return result;

	return vk::Result();
}

vk::Result vcm::StagingBuffer::writeTexture2dData(vk::Queue queue, vk::CommandBuffer commandBuffer,
	std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::Semaphore> signalSemaphores, vk::Fence fence, vk::Image texture2d,
	Image const &texture2dImageData) const {
	auto&& imageData = texture2dImageData.getData();
	auto imageDataSize = static_cast<size_t>(imageData.width * imageData.height * 4);

	vma::BufferWriteInfo bufferWriteInfo{
		.dstBuffer = *stagingBuffer,
		.dstBufferOffset = 0,
		.srcData = imageData.data,
		.dataSize = imageDataSize
	};

	if (auto writeToStagingBufferResult = memoryToBufferCopier.writeBufferFromHost(bufferWriteInfo); writeToStagingBufferResult != vk::Result::eSuccess) {
		return writeToStagingBufferResult;
	}

	if (auto commandBufferResetResult = commandBuffer.reset(vk::CommandBufferResetFlags{}); commandBufferResetResult != vk::Result::eSuccess) {
		return commandBufferResetResult;
	}

	if (auto commandBufferBeginResult = commandBuffer.begin(vk::CommandBufferBeginInfo{}); commandBufferBeginResult != vk::Result::eSuccess) {
		return commandBufferBeginResult;
	}

	commandBuffer.pipelineBarrier(
	vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {},
		{},
		{},
		vk::ImageMemoryBarrier{
		.srcAccessMask = {},
		.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = texture2d,
		.subresourceRange = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		 }
		 }
	);
	commandBuffer.copyBufferToImage(stagingBuffer.get(), texture2d, vk::ImageLayout::eTransferDstOptimal, vk::BufferImageCopy{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = vk::ImageSubresourceLayers{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = vk::Offset3D{ .x = 0, .y = 0, .z = 0 },
		.imageExtent = vk::Extent3D{ .width = (imageData.width), .height = (imageData.height), .depth = 1 }
	});
	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
		{},
		{},
		vk::ImageMemoryBarrier{
			.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
			.dstAccessMask = vk::AccessFlagBits::eShaderRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = texture2d,
			.subresourceRange = vk::ImageSubresourceRange{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		}
	);
	if (auto commandBufferEndResult = commandBuffer.end(); commandBufferEndResult != vk::Result::eSuccess) {
		return commandBufferEndResult;
	}

	std::vector<vk::PipelineStageFlags> submitPipelineStages{ waitSemaphores.size(), vk::PipelineStageFlagBits::eTopOfPipe };

	if (auto queueSubmitResult = queue.submit(
		vk::SubmitInfo{
			.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
			.pWaitSemaphores = waitSemaphores.data(),
			.pWaitDstStageMask = submitPipelineStages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
			.pSignalSemaphores = signalSemaphores.data()
		}, fence); queueSubmitResult != vk::Result::eSuccess) {
		return queueSubmitResult;
	}
	return vk::Result::eSuccess;
}

vk::Result vcm::StagingBuffer::generateMipmaps(vk::Queue queue, vk::CommandBuffer commandBuffer,
	std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::Semaphore> signalSemaphores, vk::Fence fence,
	vk::Image texture2d, vk::Extent2D imageExtent, uint32_t mipmapLevels) {

	if (auto resetResult = commandBuffer.reset(); resetResult != vk::Result::eSuccess) {
		return resetResult;
	}
	if (auto beginResult = commandBuffer.begin(vk::CommandBufferBeginInfo{}); beginResult != vk::Result::eSuccess) {
		return beginResult;
	}

	auto imageWidth = static_cast<int32_t>(imageExtent.width);
	auto imageHeight = static_cast<int32_t>(imageExtent.height);

	for ( auto i = 0U; i < mipmapLevels - 1; i++ ) {

		commandBuffer.pipelineBarrier(

			i == 0 ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eTransfer, {},
			{},
			{},
			{
				vk::ImageMemoryBarrier{
					.srcAccessMask = i == 0 ? vk::AccessFlags{} : vk::AccessFlagBits::eTransferWrite,
					.dstAccessMask = vk::AccessFlagBits::eTransferRead,
					.oldLayout = i == 0 ? vk::ImageLayout::eUndefined : vk::ImageLayout::eTransferDstOptimal,
					.newLayout = vk::ImageLayout::eTransferSrcOptimal,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = texture2d,
					.subresourceRange = vk::ImageSubresourceRange{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.baseMipLevel = i,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				},
				vk::ImageMemoryBarrier{
					.srcAccessMask = vk::AccessFlags{},
					.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eTransferDstOptimal,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = texture2d,
					.subresourceRange = vk::ImageSubresourceRange{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.baseMipLevel = i + 1,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				}
			}
		);

		commandBuffer.blitImage(
			texture2d, vk::ImageLayout::eTransferSrcOptimal,
			texture2d, vk::ImageLayout::eTransferDstOptimal,
			vk::ImageBlit{
				.srcSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = i,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.srcOffsets = std::array{
					vk::Offset3D{ .x = 0, .y = 0, .z = 0 },
					vk::Offset3D{ .x = imageWidth, .y = imageHeight, .z = 1 }
				},
				.dstSubresource = vk::ImageSubresourceLayers{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = i + 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.dstOffsets = std::array{
					vk::Offset3D{ .x = 0, .y = 0, .z = 0 },
					vk::Offset3D{ .x = imageWidth/2, .y = imageHeight/2, .z = 1 }
				}
			},
			vk::Filter::eLinear
		);

		imageWidth /= 2;
		imageHeight /= 2;
	}

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eAllCommands,
		{},
		{},
		{},
		{
			vk::ImageMemoryBarrier{
				.srcAccessMask = vk::AccessFlagBits::eTransferRead,
				.dstAccessMask = vk::AccessFlagBits::eShaderRead,
				.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = texture2d,
				.subresourceRange = vk::ImageSubresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			},
			vk::ImageMemoryBarrier{
				.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
				.dstAccessMask = {},
				.oldLayout = vk::ImageLayout::eTransferDstOptimal,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = texture2d,
				.subresourceRange = vk::ImageSubresourceRange{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 1,
					.levelCount = mipmapLevels - 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			}
		}
	);

	if (auto endResult = commandBuffer.end(); endResult != vk::Result::eSuccess) {
		return endResult;
	}

	std::vector<vk::PipelineStageFlags> submitPipelineStages{ waitSemaphores.size(), vk::PipelineStageFlagBits::eTopOfPipe };

	if (auto submitResult = queue.submit(
		vk::SubmitInfo{
			.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
			.pWaitSemaphores = waitSemaphores.data(),
			.pWaitDstStageMask = submitPipelineStages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
			.pSignalSemaphores = signalSemaphores.data()
		}, fence); submitResult != vk::Result::eSuccess) {
		return submitResult;
	}

	return vk::Result::eSuccess;

}
