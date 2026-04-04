#include "VulkanCopyManager.hpp"

vk::Result vcm::StagingBuffer::writeCubeMapData(
	vk::Queue queue, 
	vk::CommandBuffer commandBuffer, 
	std::vector<vk::Semaphore> waitSemaphore, 
	std::vector<vk::Semaphore> signalSemaphore, 
	vk::Image cubeMap, 
	CubeMapImage const& image) const
{
	uint32_t sideWidth = static_cast<uint32_t>(image.getData().sideWidth);
	size_t imageSideDataSize = static_cast<size_t>(image.getData().sideWidth * image.getData().sideWidth * 4);
	size_t imageDataSize = static_cast<size_t>(image.getData().sideWidth * image.getData().sideWidth * 4) * 6;

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

	if (auto result = queue.submit(
		vk::SubmitInfo{
			.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphore.size()),
			.pWaitSemaphores = waitSemaphore.data(),
			.pWaitDstStageMask = std::vector<vk::PipelineStageFlags>(waitSemaphore.size(), vk::PipelineStageFlagBits::eColorAttachmentOutput).data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphore.size()),
			.pSignalSemaphores = signalSemaphore.data()
		}); result != vk::Result::eSuccess) return result;

	return vk::Result();
}