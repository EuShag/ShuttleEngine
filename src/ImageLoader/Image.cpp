#define STB_IMAGE_IMPLEMENTATION
#include "Image.hpp"
#include "stb_image.h"

Image::Image(std::string const& filePath) {
	int width, height, channels;
	imageData.data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	imageData.width = width;
	imageData.height = height;
}

struct ExtentAndChannels {
	int width;
	int height;
	int channels;
};

CubeMapImage::CubeMapImage(CubeMapImageFiles const& cubeMapImageFiles) {

	ExtentAndChannels rightExtentAndChannels{};
	ExtentAndChannels leftExtentAndChannels{};
	ExtentAndChannels topExtentAndChannels{};
	ExtentAndChannels bottomExtentAndChannels{};
	ExtentAndChannels frontExtentAndChannels{};
	ExtentAndChannels backExtentAndChannels{};

	imageData.rightData = stbi_load(cubeMapImageFiles.rightFilePath.c_str(), &rightExtentAndChannels.width, &rightExtentAndChannels.height, &rightExtentAndChannels.channels, 4);
	imageData.leftData = stbi_load(cubeMapImageFiles.leftFilePath.c_str(), &leftExtentAndChannels.width, &leftExtentAndChannels.height, &leftExtentAndChannels.channels, 4);
	imageData.topData = stbi_load(cubeMapImageFiles.topFilePath.c_str(), &topExtentAndChannels.width, &topExtentAndChannels.height, &topExtentAndChannels.channels, 4);
	imageData.bottomData = stbi_load(cubeMapImageFiles.bottomFilePath.c_str(), &bottomExtentAndChannels.width, &bottomExtentAndChannels.height, &bottomExtentAndChannels.channels, 4);
	imageData.frontData = stbi_load(cubeMapImageFiles.frontFilePath.c_str(), &frontExtentAndChannels.width, &frontExtentAndChannels.height, &frontExtentAndChannels.channels, 4);
	imageData.backData = stbi_load(cubeMapImageFiles.backFilePath.c_str(), &backExtentAndChannels.width, &backExtentAndChannels.height, &backExtentAndChannels.channels, 4);

	if (
		rightExtentAndChannels.width != rightExtentAndChannels.height || 
		leftExtentAndChannels.width != leftExtentAndChannels.height ||
		topExtentAndChannels.width != topExtentAndChannels.height ||
		bottomExtentAndChannels.width != bottomExtentAndChannels.height ||
		frontExtentAndChannels.width != frontExtentAndChannels.height ||
		backExtentAndChannels.width != backExtentAndChannels.height
		)
		throw std::runtime_error("All cube map images must be square");

	if (
		rightExtentAndChannels.width != leftExtentAndChannels.width ||
		rightExtentAndChannels.width != topExtentAndChannels.width ||
		rightExtentAndChannels.width != bottomExtentAndChannels.width ||
		rightExtentAndChannels.width != frontExtentAndChannels.width ||
		rightExtentAndChannels.width != backExtentAndChannels.width
		)
		throw std::runtime_error("All cube map images must have the same width");

	imageData.sideWidth = rightExtentAndChannels.width;
}
