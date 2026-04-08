#pragma once
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include "IncludeVulkan.hpp"

struct ImageData {
	uint32_t width = 0;
	uint32_t height = 0;
	unsigned char* data = nullptr;
};

struct CubeMapImageData {
	uint32_t sideWidth = 0;
	void* rightData = nullptr;
	void* leftData = nullptr;
	void* topData = nullptr;
	void* bottomData = nullptr;
	void* backData = nullptr;
	void* frontData = nullptr;
};

struct CubeMapImageFiles {
	std::string rightFilePath;
	std::string leftFilePath;
	std::string topFilePath;
	std::string bottomFilePath;
	std::string frontFilePath;
	std::string backFilePath;
};

struct StagingBufferData {
	vk::Buffer buffer;
	vk::DeviceMemory memory;
	vk::DeviceSize offset;
};

class Image {
public:
	explicit Image(std::string const& filePath);

	[[nodiscard]] ImageData getData() const { return imageData; }
	[[nodiscard]] size_t getTotalSize() const { return imageData.width * imageData.height * 4; }
private:
	ImageData imageData{.width = 0, .height = 0, .data = nullptr};
};

class CubeMapImage {
public:
	CubeMapImage() = delete;
	explicit CubeMapImage(CubeMapImageFiles const& cubeMapImageFiles);
	[[nodiscard]] CubeMapImageData getData() const { return imageData; }
	[[nodiscard]] size_t getTotalDataSize() const { return static_cast<size_t>(imageData.sideWidth * imageData.sideWidth * 4) * 6; }

private:
	CubeMapImageData imageData{
		.sideWidth = 0,
		.rightData = nullptr, .leftData = nullptr, 
		.topData = nullptr, .bottomData = nullptr, 
		.backData = nullptr, .frontData = nullptr
	};
};