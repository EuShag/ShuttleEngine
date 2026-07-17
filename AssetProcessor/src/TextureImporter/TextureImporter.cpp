#include "TextureImporter.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

#include <omp.h>

// 1. Подключаем STB
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define TINYDDSLOADER_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize2.h"
#include "bc7enc.h"
#include "rgbcx.h"
#include "tinyddsloader.h"
#include <ktx.h>

namespace shuttle_engine::compiler {

    // Вспомогательный метод для получения блока 4x4
    static void get4x4Block(const uint8_t* rgba, int width, int height, int blockX, int blockY, uint32_t* outBlock) {
        for (int y = 0; y < 4; ++y) {
            int py = std::min(blockY * 4 + y, height - 1);
            for (int x = 0; x < 4; ++x) {
                int px = std::min(blockX * 4 + x, width - 1);
                size_t pixelIdx = (py * width + px) * 4;

                uint8_t r = rgba[pixelIdx + 0];
                uint8_t g = rgba[pixelIdx + 1];
                uint8_t b = rgba[pixelIdx + 2];
                uint8_t a = rgba[pixelIdx + 3];

                outBlock[y * 4 + x] = r | (g << 8) | (b << 16) | (a << 24);
            }
        }
    }

    static void renormalizeNormalMap(std::vector<uint8_t>& rgbaPixels, int width, int height) {
        auto const totalPixels = static_cast<size_t>(width) * height;
        auto* pixels = reinterpret_cast<glm::u8vec4*>(rgbaPixels.data());

        // #pragma omp parallel for // Если подключите OpenMP, этот цикл улетит на все ядра CPU
        #pragma omp parallel for
        for (size_t i = 0; i < totalPixels; ++i) {
            // 1. Распаковка байт в float-вектор через конструктор GLM
            auto normal = glm::vec3(pixels[i].x, pixels[i].y, pixels[i].z);

            // 2. Перевод в диапазон [-1.0, 1.0]
            normal = normal / 255.0f * 2.0f - 1.0f;

            // 3. Быстрая нормализация средствами GLM (внутри используются SIMD-инструкции)
            if (auto const lengthSq = glm::dot(normal, normal); lengthSq > 0.0001f) {
                normal = glm::inversesqrt(lengthSq) * normal; // inversesqrt использует тот самый быстрый _mm_rsqrt_ps
            } else {
                normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            // 4. Упаковка обратно в байты
            pixels[i].x = static_cast<uint8_t>((normal.x + 1.0f) * 0.5f * 255.0f);
            pixels[i].y = static_cast<uint8_t>((normal.y + 1.0f) * 0.5f * 255.0f);
            pixels[i].z = static_cast<uint8_t>((normal.z + 1.0f) * 0.5f * 255.0f);
            // Альфа-канал pixels[i].w остается нетронутым!
        }
    }

    // -----------------------------------------------------------------------------
    // ТОЧКА ВХОДА 1: ИМПОРТ ИЗ ФАЙЛА (Загружает файл в память и вызывает перегрузку)
    // -----------------------------------------------------------------------------
    bool TextureImporter::import(
        const std::string& filePath,
        const TextureImportOptions& options,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData
    ) {
        // Читаем файл в память за один проход (быстрый бинарный буфер)
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[TextureImporter] Failed to open file: " << filePath << std::endl;
            return false;
        }

        std::streamsize const size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileBuffer(size);
        if (!file.read(reinterpret_cast<char*>(fileBuffer.data()), size)) {
            return false;
        }

        // Определяем расширение для подсказки формата
        auto const formatHint = filePath.substr(filePath.find_last_of('.') + 1);

        // Просто перенаправляем данные во вторую перегрузку!
        return import(fileBuffer.data(), fileBuffer.size(), formatHint, options, outMetaData, outData);
    }

    // -----------------------------------------------------------------------------
    // ТОЧКА ВХОДА 2: ИМПОРТ ИЗ ПАМЯТИ (Единый обработчик всей логики)
    // -----------------------------------------------------------------------------
    bool TextureImporter::import(
        const uint8_t* memoryData,
        size_t memorySize,
        const std::string& formatHint,
        const TextureImportOptions& options,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData
    ) {
        std::string ext = formatHint;
        std::ranges::transform(ext, ext.begin(), ::tolower);

        if (ext == "dds") {
            return importDDS(memoryData, memorySize, outMetaData, outData);
        }
        else if (ext == "ktx" || ext == "ktx2") {
            return importKTX(memoryData, memorySize, outMetaData, outData);
        }
        else {
            return importSTB(memoryData, memorySize, options, outMetaData, outData);
        }
    }

    // -----------------------------------------------------------------------------
    // ПАЙПЛАЙН 1: СЫРЫЕ ТЕКСТУРЫ (STB + COMPRESSION)
    // -----------------------------------------------------------------------------
    bool TextureImporter::importSTB(
        const uint8_t* data,
        size_t size,
        const TextureImportOptions& options,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData
    ) {
        stbi_set_flip_vertically_on_load(options.flipY ? 1 : 0);
        int w, h, comp;
        uint8_t* rawPixels = stbi_load_from_memory(data, size, &w, &h, &comp, 4); // Всегда RGBA
        if (!rawPixels) return false;

        uint32_t mipCount = options.generateMips ?
            static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1 : 1;

        bc7enc_compress_block_init();
        rgbcx::init();

        outData.clear();
        std::vector currentMipPixels(rawPixels, rawPixels + (w * h * 4));
        stbi_image_free(rawPixels);

        int curW = w;
        int curH = h;

        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            std::vector<uint8_t> compressedLevel;

            if (options.format == TextureFormat::BC7_SRGB) {
                compressedLevel = compressToBC7(currentMipPixels.data(), curW, curH);
            } else if (options.format == TextureFormat::BC5_UNORM) {
                renormalizeNormalMap(currentMipPixels, curW, curH);
                compressedLevel = compressToBC5(currentMipPixels.data(), curW, curH);
            }

            outData.insert(outData.end(), compressedLevel.begin(), compressedLevel.end());

            if (mip < mipCount - 1) {
                int nextW = std::max(1, curW / 2);
                int nextH = std::max(1, curH / 2);
                std::vector<uint8_t> nextMipPixels(nextW * nextH * 4);

                stbir_resize_uint8_linear(
                    currentMipPixels.data(), curW, curH, 0,
                    nextMipPixels.data(), nextW, nextH, 0,
                    STBIR_RGBA
                );

                currentMipPixels = std::move(nextMipPixels);
                curW = nextW;
                curH = nextH;
            }
        }

        outMetaData.width = w;
        outMetaData.height = h;
        outMetaData.mipCount = mipCount;
        outMetaData.numLayers = 1;
        outMetaData.isCubemap = 0;
        outMetaData.format = (options.format == TextureFormat::BC7_SRGB) ? 145 : 141; // Vulkan BC7/BC5

        return true;
    }

    std::vector<uint8_t> TextureImporter::compressToBC7(const uint8_t* rgbaPixels, int width, int height) {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        std::vector<uint8_t> outData(blocksX * blocksY * 16);

        bc7enc_compress_block_params params{};
        bc7enc_compress_block_params_init(&params);
        params.m_weights[0] = 1.0f; // Red   [0]
        params.m_weights[1] = 1.0f; // Green [1]
        params.m_weights[2] = 1.0f; // Blue  [2]
        params.m_weights[3] = 1.0f; // Alpha [3]

        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                uint32_t rgbaBlock[16];
                get4x4Block(rgbaPixels, width, height, bx, by, rgbaBlock);
                uint8_t* dst = outData.data() + (by * blocksX + bx) * 16;
                bc7enc_compress_block(dst, rgbaBlock, &params);
            }
        }
        return outData;
    }


    std::vector<uint8_t> TextureImporter::compressToBC5(const uint8_t* rgbaPixels, int width, int height) {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        std::vector<uint8_t> outData(blocksX * blocksY * 16);

        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                uint32_t rgbaBlock[16];
                get4x4Block(rgbaPixels, width, height, bx, by, rgbaBlock);
                uint8_t* dst = outData.data() + (by * blocksX + bx) * 16;
                rgbcx::encode_bc5(dst, reinterpret_cast<const uint8_t*>(rgbaBlock), 0, 1, 4);
            }
        }
        return outData;
    }

    bool TextureImporter::importDDS(
        const uint8_t* data,
        size_t size,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData
    ) {
        tinyddsloader::DDSFile dds;

        if (dds.Load(data, size) != tinyddsloader::Result::Success) {
            std::cerr << "[TextureImporter] tiny_dds_loader failed to load" << std::endl;
            return false;
        }

        uint32_t vkFormat = 0; // Маппинг форматов DDS в форматы Vulkan
        auto ddsFormat = dds.GetFormat();

        // Ошибка 1: Исправлено перечисление форматов (DXGI_FORMAT_* не принадлежат tinyddsloader)
        if (ddsFormat == tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm ||
            ddsFormat == tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB) {

            // Рекомендуется использовать ENUM из vulkan.h, но если передаем числом:
            vkFormat = 145; // VK_FORMAT_BC7_SRGB_BLOCK
        }
        else if (ddsFormat == tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm) {
            vkFormat = 141; // VK_FORMAT_BC5_UNORM_BLOCK
        }
        else if (ddsFormat == tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm) {
            vkFormat = 142; // VK_FORMAT_BC5_SNORM_BLOCK (Важно разделять UNORM и SNORM!)
        }
        else {
            std::cerr << "[TextureImporter] DDS format not supported (only BC5/BC7 allowed): " << static_cast<int>(ddsFormat) << std::endl;
            return false;
        }

        outMetaData.width = dds.GetWidth();
        outMetaData.height = dds.GetHeight();
        outMetaData.mipCount = dds.GetMipCount();
        outMetaData.numLayers = dds.GetArraySize();
        outMetaData.isCubemap = dds.IsCubemap() ? 1 : 0;
        outMetaData.format = vkFormat;

        // Оптимизация: Считаем точный размер заранее, чтобы избежать постоянных реаллокаций вектора
        size_t totalSize = 0;
        for (uint32_t mip = 0; mip < dds.GetMipCount(); ++mip) {
            for (uint32_t layer = 0; layer < dds.GetArraySize(); ++layer) {
                if (const auto* imageData = dds.GetImageData(mip, layer)) {
                    totalSize += imageData->m_memSlicePitch;
                }
            }
        }

        outData.clear();
        outData.reserve(totalSize); // Выделяем память один раз

        // Упаковка данных
        for (uint32_t mip = 0; mip < dds.GetMipCount(); ++mip) {
            for (uint32_t layer = 0; layer < dds.GetArraySize(); ++layer) {
                const auto* imageData = dds.GetImageData(mip, layer);

                // Ошибка 3: Проверка на nullptr
                if (!imageData || !imageData->m_mem) {
                    std::cerr << "[TextureImporter] Mip " << mip << " Layer " << layer << " has no data!" << std::endl;
                    return false;
                }

                // Ошибка 2: Использование m_memSliceSize вместо m_memPitchSize для BC-форматов
                const auto* bytePtr = static_cast<const uint8_t*>(imageData->m_mem);
                outData.insert(outData.end(), bytePtr, bytePtr + imageData->m_memSlicePitch);
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------------
    // ПАЙПЛАЙН 3: СКВОЗНОЙ ИМПОРТ KTX/KTX2 (LIBKTX)
    // -----------------------------------------------------------------------------
    bool TextureImporter::importKTX(
        const uint8_t* data,
        size_t size,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData
    ) {
        ktxTexture* kTexture = nullptr;

        KTX_error_code result = ktxTexture_CreateFromMemory(
            data,
            size,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &kTexture
        );

        if (result != KTX_SUCCESS) {
            std::cerr << "[TextureImporter] libktx failed to load: " << data << " with code " << result << std::endl;
            return false;
        }

        // Автоматический выбор формата для транскодирования
        if (kTexture->classId == ktxTexture2_c && ktxTexture2_NeedsTranscoding(reinterpret_cast<ktxTexture2 *>(kTexture))) {

            auto* kTx2 = reinterpret_cast<ktxTexture2 *>(kTexture);
            ktx_transcode_fmt_e targetFormat{}; // Формат по умолчанию (для цвета)

            // ИСПРАВЛЕНИЕ: Используем официальные поля вместо приватного _protected

            // В KTX2 карты нормалей Basis Universal помечаются через DFD или имеют ровно 2 канала (R и G)
            // Также можно проверить имя модели распределения данных (colorModel)

            if (uint32_t const numComponents = ktxTexture2_GetNumComponents(kTx2); numComponents == 2) {
                targetFormat = KTX_TTF_BC5_RG; // Автоматически транскодируем нормали в BC5!
            }
            else if (numComponents == 1) {
                targetFormat = KTX_TTF_BC4_R;  // Если это маска или карта высот (один канал), жмем в BC4
            }
            else {
                // Если это обычный RGB/RGBA цвет, то смотрим на альфа-канал
                // Если альфы нет, можно выставить KTX_TTF_BC1_RGB (для экономии памяти),
                // но BC7_RGBA (KTX_TTF_BC7_RGBA) — это универсальный и самый качественный выбор для Vulkan
                targetFormat = KTX_TTF_BC7_RGBA;
            }

            // 3. Запускаем транскодирование в автоматически определенный формат

            if (auto const transcodeResult = ktxTexture2_TranscodeBasis(kTx2, targetFormat, 0); transcodeResult != KTX_SUCCESS) {
                std::cerr << "[TextureImporter] Автоматический транскодинг провален!" << std::endl;
                ktxTexture_Destroy(kTexture);
                return false;
            }
        }

        // 4. Теперь vkFormat ГАРАНТИРОВАННО вернет правильный нативный формат Vulkan
        // (145 для BC7, 141 для BC5_UNORM и т.д.), который выбрала сама библиотека после транскодирования!
        auto const vkFormat = reinterpret_cast<ktxTexture2*>(kTexture)->vkFormat;

        // Добавляем 142 (BC5_SNORM), так как libktx часто используется для нормалей
        if (vkFormat != 145 && vkFormat != 141 && vkFormat != 142) {
            std::cerr << "[TextureImporter] KTX format not supported (only BC5/BC7 allowed): " << vkFormat << std::endl;
            ktxTexture_Destroy(kTexture);
            return false;
        }

        // Заполняем метаданные
        outMetaData.width     = kTexture->baseWidth;
        outMetaData.height    = kTexture->baseHeight;
        outMetaData.mipCount  = kTexture->numLevels;
        outMetaData.numLayers = kTexture->numLayers;
        outMetaData.isCubemap = kTexture->isCubemap ? 1 : 0;
        outMetaData.format    = vkFormat;

        // Ошибка: Прямой assign копирует KTX-выравнивание (4-byte padding)
        // Решение: Считаем чистый размер без выравнивания и собираем данные вручную

        uint32_t numLevels = kTexture->numLevels;
        uint32_t numLayers = kTexture->numLayers;
        // Если это кубмапа, libktx хранит грани как "faces", обрабатываем их как слои
        uint32_t numFaces  = kTexture->numFaces;
        uint32_t totalLayers = (numLayers > 1) ? numLayers : numFaces;

        // Сначала считаем точный чистый размер данных
        size_t cleanTotalSize = 0;
        for (uint32_t mip = 0; mip < numLevels; ++mip) {
            // Размер одного изображения на данном мип-уровне без учета выравнивания KTX
            size_t levelSize = ktxTexture_GetImageSize(kTexture, mip);
            cleanTotalSize += levelSize * totalLayers;
        }

        outData.clear();
        outData.reserve(cleanTotalSize);

        // Упаковываем строго по вашей схеме: Mip 0 (все слои), Mip 1 (все слои) и т.д.
        for (uint32_t mip = 0; mip < numLevels; ++mip) {
            size_t imageSize = ktxTexture_GetImageSize(kTexture, mip);

            for (uint32_t layer = 0; layer < totalLayers; ++layer) {
                size_t offset = 0;
                KTX_error_code r;

                // Находим точное смещение этого изображения внутри KTX (учитывая внутренний паддинг)
                if (kTexture->isCubemap) {
                    r = ktxTexture_GetImageOffset(kTexture, mip, 0, layer, &offset);
                } else {
                    r = ktxTexture_GetImageOffset(kTexture, mip, layer, 0, &offset);
                }

                if (r != KTX_SUCCESS) {
                    std::cerr << "[TextureImporter] Failed to get image offset for Mip " << mip << " Layer " << layer << std::endl;
                    ktxTexture_Destroy(kTexture);
                    return false;
                }

                // Берем указатель на начало чистых данных этого слоя/мипа
                const uint8_t* imagePtr = ktxTexture_GetData(kTexture) + offset;

                // Копируем ровно imageSize байт (без мусора выравнивания)
                outData.insert(outData.end(), imagePtr, imagePtr + imageSize);
            }
        }

        ktxTexture_Destroy(kTexture);
        return true;
    }


} // namespace shuttle_engine::compiler