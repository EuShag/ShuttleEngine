#pragma once
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include "IncludeVulkan.hpp"

struct ImageData {
	int width;
	int height;
	int channels;
	unsigned char* data;
};

struct CubeMapImageData {
	int sideWidth;
	int reserved = 0;
	void* rightData;
	void* leftData;
	void* topData;
	void* bottomData;
	void* frontData;
	void* backData;
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
private:
	ImageData imageData{.width = 0, .height = 0, .channels = 0, .data = nullptr};
};

class CubeMapImage {
public:
	CubeMapImage() = delete;
	explicit CubeMapImage(CubeMapImageFiles const& cubeMapImageFiles);
	[[nodiscard]] CubeMapImageData getData() const { return imageData; }

private:
	CubeMapImageData imageData{
		.sideWidth = 0, 
		.rightData = nullptr, .leftData = nullptr, 
		.topData = nullptr, .bottomData = nullptr,
		.frontData = nullptr, .backData = nullptr
	};
};