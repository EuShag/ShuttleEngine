#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "BlobLayout.hpp" // Твой заголовочный файл со структурами
#include <optional>

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

        // Упаковать раздельные карты Occlusion / Roughness / Metalness в одну ORM текстуру.
        // Каждый путь может быть std::nullopt, тогда соответствующий канал будет заполнен дефолтным значением.
        static void packORMFromFiles(const std::optional<std::string>& occlusionPath,
                                     const std::optional<std::string>& roughnessPath,
                                     const std::optional<std::string>& metalnessPath,
                                     const TextureImportOptions& options,
                                     format::TextureMetaData& outMetaData,
                                     std::vector<uint8_t>& outData,
                                     bool roughnessIsGloss = false);

        // Альтернатива: упаковать ORM из данных в памяти (встроенные Assimp-текстуры)
        // Каждый блок памяти может быть nullptr/size==0 чтобы обозначить отсутствие.
        static void packORMFromMemory(const uint8_t* occlusionData, size_t occlusionSize,
                                      const uint8_t* roughnessData, size_t roughnessSize,
                                      const uint8_t* metalnessData, size_t metalnessSize,
                                      const TextureImportOptions& options,
                                      format::TextureMetaData& outMetaData,
                                      std::vector<uint8_t>& outData,
                                      bool roughnessIsGloss = false);

        // Импорт напрямую из RGBA8-памяти (Assimp может предоставить несжатые встроенные текстуры)
        static bool importFromRGBA(const uint8_t* rgbaPixels, int width, int height, const TextureImportOptions& options, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData);

        // Инициализация внешних библиотек (bc7/rgbcx) — вызывается перед массовым параллельным импортом
        static void initialize();

    private:
        // Пайплайн 1: Обработка сырых картинок (stb + bc7enc/rgbcx)
        static bool importSTB(const uint8_t* data, size_t size, const TextureImportOptions& options, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData);
        static bool importDDS(const uint8_t* data, size_t size, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData);

        static std::vector<uint8_t> compressToBC7(const uint8_t* rgbaPixels, int width, int height);
        static std::vector<uint8_t> compressToBC5(const uint8_t* rgbaPixels, int width, int height);
    };

} // namespace shuttle_engine::compiler
