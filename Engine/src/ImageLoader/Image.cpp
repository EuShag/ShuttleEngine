#define STB_IMAGE_IMPLEMENTATION
#include "Image.hpp"
#include "stb_image.h"
#include <algorithm>
#include <stdexcept>

Image1D16bit::Image1D16bit(std::string const &filePath) {
	int width, height, channels;
	imageData.data = stbi_load_16(filePath.c_str(), &width, &height, &channels, 1);
	imageData.width = width;
	imageData.height = height;
}

uint16_t Image1D16bit::getTexelValue(uint32_t x, uint32_t y) const {
	if (!imageData.data || imageData.width == 0 || imageData.height == 0) {
		return 0;
	}
	x = std::clamp(x, 0u, imageData.width - 1);
	y = std::clamp(y, 0u, imageData.height - 1);
	return imageData.data[y * imageData.width + x];
}

float Image1D16bit::getTexelValueNormalized(uint32_t x, uint32_t y) const {
	if (!imageData.data || imageData.width == 0 || imageData.height == 0) {
		return 0.0f;
	}
	x = std::clamp(x, 0u, imageData.width - 1);
	y = std::clamp(y, 0u, imageData.height - 1);
	return imageData.data[y * imageData.width + x] / 65536.0f;
}

float Image1D16bit::sampleBilinear(float u, float v) const {
	if (!imageData.data || imageData.width == 0 || imageData.height == 0) {
		return 0.0f;
	}
	u = std::clamp(u, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

	float x_pixel = u * (imageData.width - 1);
	float y_pixel = v * (imageData.height - 1);

	uint32_t x0 = static_cast<uint32_t>(x_pixel);
	uint32_t y0 = static_cast<uint32_t>(y_pixel);
	uint32_t x1 = std::min(x0 + 1, imageData.width - 1);
	uint32_t y1 = std::min(y0 + 1, imageData.height - 1);

	float val00 = getTexelValueNormalized(x0, y0);
	float val10 = getTexelValueNormalized(x1, y0);
	float val01 = getTexelValueNormalized(x0, y1);
	float val11 = getTexelValueNormalized(x1, y1);

	float dx = x_pixel - x0;
	float dy = y_pixel - y0;

	float top    = val00 * (1.0f - dx) + val10 * dx;
	float bottom = val01 * (1.0f - dx) + val11 * dx;
	return top * (1.0f - dy) + bottom * dy;
}

Image1D16bit::~Image1D16bit() {
	stbi_image_free(imageData.data);
}
