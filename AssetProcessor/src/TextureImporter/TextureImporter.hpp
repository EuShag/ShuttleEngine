#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "BlobLayout.hpp" // Твой заголовочный файл со структурами

namespace shuttle_engine::compiler {

    enum class TextureFormat {
        BC7_SRGB,  // Для цвета (Albedo) и карт материалов (ORM)
        BC5_UNORM  // Для двухканальных карт нормалей (DirectX)
    };

    struct TextureImportOptions {
        TextureFormat format = TextureFormat::BC7_SRGB;
        bool generateMips = true;
        bool flipY = false;
    };

    class TextureImporter {
    public:
        // Точка входа 1: Импорт из файла (делает маппинг/чтение файла в память)
        static bool import(
            const std::string& filePath,
            const TextureImportOptions& options,
            format::TextureMetaData& outMetaData,
            std::vector<uint8_t>& outData
        );

        // Точка входа 2: Импорт напрямую из памяти (для встроенных текстур glTF/FBX)
        static bool import(
            const uint8_t* memoryData,
            size_t memorySize,
            const std::string& formatHint,
            const TextureImportOptions& options,
            format::TextureMetaData& outMetaData,
            std::vector<uint8_t>& outData
        );

    private:
        // Пайплайн 1: Обработка сырых картинок (stb + bc7enc/rgbcx)
        static bool importSTB(const uint8_t* data, size_t size, const TextureImportOptions& options, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData);
        static bool importDDS(const uint8_t* data, size_t size, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData);
        static bool importKTX(const uint8_t* data, size_t size, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData);

        static std::vector<uint8_t> compressToBC7(const uint8_t* rgbaPixels, int width, int height);
        static std::vector<uint8_t> compressToBC5(const uint8_t* rgbaPixels, int width, int height);
    };

} // namespace shuttle_engine::compiler
