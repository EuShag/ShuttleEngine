#define STB_IMAGE_IMPLEMENTATION
#include "Image.hpp"
#include "stb_image.h"

Image::Image(std::string const& filePath) {
	imageData.data = stbi_load(filePath.c_str(), &imageData.width, &imageData.height, &imageData.channels, 4);
}

CubeMapImage::CubeMapImage(CubeMapImageFiles const& cubeMapImageFiles) {
	imageData.rightData = stbi_load(cubeMapImageFiles.rightFilePath.c_str(), &imageData.sideWidth, &imageData.sideWidth, nullptr, 4);
	imageData.leftData = stbi_load(cubeMapImageFiles.leftFilePath.c_str(), nullptr, nullptr, nullptr, 4);
	imageData.topData = stbi_load(cubeMapImageFiles.topFilePath.c_str(), nullptr, nullptr, nullptr, 4);
	imageData.bottomData = stbi_load(cubeMapImageFiles.bottomFilePath.c_str(), nullptr, nullptr, nullptr, 4);
	imageData.frontData = stbi_load(cubeMapImageFiles.frontFilePath.c_str(), nullptr, nullptr, nullptr, 4);
	imageData.backData = stbi_load(cubeMapImageFiles.backFilePath.c_str(), nullptr, nullptr, nullptr, 4);

}
