#define STB_IMAGE_IMPLEMENTATION
#include "Image.hpp"
#include "stb_image.h"

Image::Image(std::string const& filePath) {
	int width, height, channels;
	imageData.data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	imageData.width = width;
	imageData.height = height;

}

Image::~Image() {
	stbi_image_free(imageData.data);
}

shuttle_engine::HostImageData loadImageFromFile(std::string filePath, vk::Format format = vk::Format::eR8G8B8A8Unorm) {
	int width, height, channels;
	// Форсируем 4 канала (RGBA) для предсказуемости в Vulkan
	unsigned char* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error("shuttle_engine::loadImageFromFile: Failed to load image from path: " + filePath);
	}

	shuttle_engine::HostImageData rawImage;
	rawImage.width = static_cast<uint32_t>(width);
	rawImage.height = static_cast<uint32_t>(height);
	rawImage.imageFormat = format;

	size_t const dataSize = width * height * 4;
	rawImage.data.assign(pixels, pixels + dataSize);

	stbi_image_free(pixels);
	return rawImage;
}

shuttle_engine::HostImageData loadImageFromMemory(std::vector<unsigned char> const& imageData, vk::Format format) {
	int width, height, channels;
	// Загружаем из бинарного буфера в памяти
	unsigned char* pixels = stbi_load_from_memory(
		imageData.data(),
		static_cast<int>(imageData.size()),
		&width, &height, &channels,
		STBI_rgb_alpha
	);

	if (!pixels) {
		throw std::runtime_error("shuttle_engine::loadImageFromMemory: Failed to parse image from memory buffer");
	}

	shuttle_engine::HostImageData rawImage;
	rawImage.width = static_cast<uint32_t>(width);
	rawImage.height = static_cast<uint32_t>(height);
	rawImage.imageFormat = format;

	size_t const dataSize = width * height * 4;
	rawImage.data.assign(pixels, pixels + dataSize);

	stbi_image_free(pixels);
	return rawImage;
}

std::optional<shuttle_engine::HostImageData> uniteSeparatedTexturesArm(
	std::optional<shuttle_engine::HostImageData> const &ambientTextureOpt,
	std::optional<shuttle_engine::HostImageData> const &roughnessTextureOpt,
	std::optional<shuttle_engine::HostImageData> const &metallicTextureOpt)
{
	std::optional<shuttle_engine::HostImageData> armTexture;
	if (!ambientTextureOpt.has_value() && !roughnessTextureOpt.has_value() && !metallicTextureOpt.has_value()) {
		return std::nullopt; // Нет данных для объединения
	}

	if (ambientTextureOpt.has_value()) {
		armTexture.value().width = ambientTextureOpt.value().width;
		armTexture.value().height = ambientTextureOpt.value().height;
	} else if (metallicTextureOpt.has_value()) {
		armTexture.value().width = metallicTextureOpt.value().width;
		armTexture.value().height = metallicTextureOpt.value().height;
	} else if (roughnessTextureOpt.has_value()) {
		armTexture.value().width = roughnessTextureOpt.value().width;
		armTexture.value().height = roughnessTextureOpt.value().height;
	}

	// В ORM текстуре обычно 3 или 4 канала (RGB или RGBA)
	// Используем 4 байта на пиксель (RGBA), чтобы выравнивание было идеальным
	armTexture.value().imageFormat = vk::Format::eR8G8B8A8Unorm;

	// Размер данных: 4 байта на пиксель
	size_t pixelCount = armTexture.value().width * armTexture.value().height;
	armTexture.value().data.resize(pixelCount * 4);

	for (size_t i = 0; i < pixelCount; ++i) {
		// R = Ambient (Occlusion)
		if (ambientTextureOpt.has_value()) armTexture.value().data[i * 4 + 0] = ambientTextureOpt.value().data[i];
		else armTexture.value().data[i * 4 + 0] = 255;
		// G = Roughness
		if (roughnessTextureOpt.has_value()) armTexture.value().data[i * 4 + 1] = roughnessTextureOpt.value().data[i];
		else armTexture.value().data[i * 4 + 1] = 0;
		// B = Metallic
		if (metallicTextureOpt.has_value()) armTexture.value().data[i * 4 + 2] = metallicTextureOpt.value().data[i];
		else armTexture.value().data[i * 4 + 2] = 0;
		// A = 255 (полностью непрозрачная)
		armTexture.value().data[i * 4 + 3] = 255;
	}

	return armTexture;
}



Image1D16bit::Image1D16bit(std::string const &filePath) {
	int width, height, channels;
	imageData.data = stbi_load_16(filePath.c_str(), &width, &height, &channels, 1); // Загружаем только 1 канал (грейскейл)
	imageData.width = width;
	imageData.height = height;
}

// Метод для получения значения пикселя по целочисленным координатам
// Возвращает нормализованное значение (0.0 - 1.0), предполагаем одноканальное изображение
uint16_t Image1D16bit::getTexelValue(uint32_t x, uint32_t y) const {
	if (!imageData.data || imageData.width == 0 || imageData.height == 0) {
		return 0.0f; // Нет данных, возвращаем 0
	}

	// Убедимся, что координаты в пределах изображения
	// (std::clamp работает с C++17, если у тебя более старый стандарт, используй std::min/max)
	x = std::clamp(x, 0u, imageData.width - 1);
	y = std::clamp(y, 0u, imageData.height - 1);

	// Предполагаем одноканальное 8-битное изображение
	// Если channels > 1, нужно учесть это: data[(y * width + x) * channels + channel_index]
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
		return 0.0;
	}

	// Clamp U, V в диапазон [0, 1], чтобы избежать выхода за границы изображения
	u = std::clamp(u, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

	// Преобразуем нормализованные координаты в пиксельные
	// Обрати внимание: (width - 1) чтобы u=1.0 соответствовало последнему пикселю
	float x_pixel = u * (imageData.width - 1);
	float y_pixel = v * (imageData.height - 1);

	// Находим 4 ближайших пикселя
	uint32_t x0 = static_cast<uint32_t>(x_pixel);
	uint32_t y0 = static_cast<uint32_t>(y_pixel);
	uint32_t x1 = std::min(x0 + 1, imageData.width - 1); // Убеждаемся, что x1 не выходит за границу
	uint32_t y1 = std::min(y0 + 1, imageData.height - 1); // Убеждаемся, что y1 не выходит за границу

	// Если изображение 1x1, то все x0, x1, y0, y1 будут 0.
	// Если ширина или высота = 1, то x1/y1 будет равен x0/y0.

	// Получаем значения 4 пикселей
	float val00 = getTexelValueNormalized(x0, y0); // Top-left
	float val10 = getTexelValueNormalized(x1, y0); // Top-right
	float val01 = getTexelValueNormalized(x0, y1); // Bottom-left
	float val11 = getTexelValueNormalized(x1, y1); // Bottom-right

	// Вычисляем веса для интерполяции
	float dx = x_pixel - x0; // Дробная часть X
	float dy = y_pixel - y0; // Дробная часть Y

	// Билинейная интерполяция
	// 1. Интерполируем по горизонтали для верхней строки (y0)
	float interpolated_top = val00 * (1.0f - dx) + val10 * dx;

	// 2. Интерполируем по горизонтали для нижней строки (y1)
	float interpolated_bottom = val01 * (1.0f - dx) + val11 * dx;

	// 3. Интерполируем по вертикали между двумя горизонтальными результатами
	return interpolated_top * (1.0f - dy) + interpolated_bottom * dy;
}

Image1D16bit::~Image1D16bit() {
	stbi_image_free(imageData.data);
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

// Вспомогательный метод для загрузки одной текстуры через stb_image
static shuttle_engine::HostImageData loadSingleTexture(const std::string& path, vk::Format format) {
    int width, height, channels;
    // Всегда форсируем 4 канала (RGBA) для простоты работы в Vulkan
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("RewMaterialLoader: Failed to load texture at " + path);
    }

    shuttle_engine::HostImageData rawImage;
    rawImage.width = static_cast<uint32_t>(width);
    rawImage.height = static_cast<uint32_t>(height);
    rawImage.imageFormat = format;

    // Копируем данные из сырого указателя в безопасный std::vector
    size_t dataSize = width * height * 4;
    rawImage.data.assign(pixels, pixels + dataSize);

    stbi_image_free(pixels);
    return rawImage;
}

// 1. Метод загрузки готового ORM (3 текстуры)
shuttle_engine::HostMaterialData loadFromFiles(std::string const& albedoPath, std::string const& normalPath, std::string const& ormPath, std::string const& emissionPath) {
    shuttle_engine::HostMaterialData material;

    material.albedoTexture = loadSingleTexture(albedoPath, vk::Format::eR8G8B8A8Srgb);
    material.normalTexture = loadSingleTexture(normalPath, vk::Format::eR8G8B8A8Unorm);
    material.ormTexture    = loadSingleTexture(ormPath, vk::Format::eR8G8B8A8Unorm);
	material.emissiveTexture = loadSingleTexture(emissionPath, vk::Format::eR8G8B8A8Unorm);

    return material;
}

// 2. Метод сборки ORM из отдельных файлов (5 текстур)
shuttle_engine::HostMaterialData loadFromFiles(std::string const& albedoPath, std::string const& normalPath,
                                                 std::string const& roughnessPath, std::string const& occlusionPath, std::string const& metallicPath, std::string const& emissionPath) {
    shuttle_engine::HostMaterialData material;

    // Загружаем Albedo и Normal как обычно
    material.albedoTexture = loadSingleTexture(albedoPath, vk::Format::eR8G8B8A8Srgb);
    material.normalTexture = loadSingleTexture(normalPath, vk::Format::eR8G8B8A8Unorm);
	if (emissionPath.empty()) material.emissiveTexture = std::nullopt;
	else material.emissiveTexture = loadSingleTexture(emissionPath, vk::Format::eR8G8B8A8Unorm);

    // Для создания ORM загружаем отдельные карты как Grayscale (1 канал), чтобы сэкономить CPU RAM при чтении
    int w_ao, h_ao, ch_ao;
    unsigned char* ao_pixels = stbi_load(occlusionPath.c_str(), &w_ao, &h_ao, &ch_ao, STBI_grey);

    int w_rough, h_rough, ch_rough;
    unsigned char* rough_pixels = stbi_load(roughnessPath.c_str(), &w_rough, &h_rough, &ch_rough, STBI_grey);

    int w_metal, h_metal, ch_metal;
    unsigned char* metal_pixels = stbi_load(metallicPath.c_str(), &w_metal, &h_metal, &ch_metal, STBI_grey);

    if (!ao_pixels || !rough_pixels || !metal_pixels) {
        if (ao_pixels) stbi_image_free(ao_pixels);
        if (rough_pixels) stbi_image_free(rough_pixels);
        if (metal_pixels) stbi_image_free(metal_pixels);
        throw std::runtime_error("RewMaterialLoader: Failed to load split PBR maps (Roughness/Occlusion/Metallic)");
    }

    // Будем считать, что размеры всех карт совпадают (стандарт для текстурных паков)
    auto width = static_cast<uint32_t>(w_ao);
    auto height = static_cast<uint32_t>(h_ao);

    shuttle_engine::HostImageData ormImage;
    ormImage.width = width;
    ormImage.height = height;
    ormImage.imageFormat = vk::Format::eR8G8B8A8Unorm;
    ormImage.data.resize(width * height * 4); // Выделяем под RGBA

    // Магия сборки ORM (Ambient Occlusion, Roughness, Metallic)
    for (size_t i = 0; i < width * height; ++i) {
        ormImage.data[i * 4 + 0] = ao_pixels[i];        // Red = Ambient Occlusion
        ormImage.data[i * 4 + 1] = rough_pixels[i];     // Green = Roughness
        ormImage.data[i * 4 + 2] = metal_pixels[i];     // Blue = Metallic
        ormImage.data[i * 4 + 3] = 255;                 // Alpha = Заглушка (не используется в PBR)
    }

    // Чистим временную память
    stbi_image_free(ao_pixels);
    stbi_image_free(rough_pixels);
    stbi_image_free(metal_pixels);

    // Записываем результат
    material.ormTexture = std::move(ormImage);

    return material;
}