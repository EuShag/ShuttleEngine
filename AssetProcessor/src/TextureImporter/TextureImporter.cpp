#include "TextureImporter.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cctype>

#include <omp.h>
#include <vulkan/vulkan.h>

// 1. Подключаем STB
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define TINYDDSLOADER_IMPLEMENTATION
#include <cstring>

#include "stb_image.h"
#include "stb_image_resize2.h"
#include "tinyddsloader.h"
#include <optional>
#include "../ispc_texcomp/ispc_texcomp.h"

namespace shuttle_engine::compiler {
    namespace {
        // Вспомогательный метод для получения блока 4x4
        void get4x4Block(const uint8_t* rgba, int width, int height, int blockX, int blockY, uint32_t* outBlock) {
            for (int y = 0; y < 4; ++y) {
                int const py = std::min(blockY * 4 + y, height - 1);
                for (int x = 0; x < 4; ++x) {
                    int const px = std::min(blockX * 4 + x, width - 1);
                    size_t const pixelIdx = (py * width + px) * 4;

                    auto const r = static_cast<uint32_t>(rgba[pixelIdx + 0]);
                    auto const g = static_cast<uint32_t>(rgba[pixelIdx + 1]);
                    auto const b = static_cast<uint32_t>(rgba[pixelIdx + 2]);
                    auto const a = static_cast<uint32_t>(rgba[pixelIdx + 3]);

                    outBlock[y * 4 + x] = (r) | (g << 8) | (b << 16) | (a << 24);
                }
            }
        }
    }

    static void renormalizeNormalMap(std::vector<uint8_t>& rgbaPixels, int width, int height) {
        auto const totalPixels = static_cast<size_t>(width) * height;
        auto* pixels = reinterpret_cast<glm::u8vec4*>(rgbaPixels.data());

        // #pragma omp parallel for // Если подключите OpenMP, этот цикл улетит на все ядра CPU
        #pragma omp parallel for default(none) shared(totalPixels, pixels)
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
        if (size <= 0) {
            std::cerr << "[TextureImporter] Empty or invalid file size: " << filePath << std::endl;
            return false;
        }
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
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        if (ext == "dds") {
            return importDDS(memoryData, memorySize, outMetaData, outData);
        }
        if (ext == "ktx" || ext == "ktx2") {
            std::cerr << "[TextureImporter] KTX format not yet implemented." << std::endl;
            return false;
        }
        if (ext == "png" || ext == "png2" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp") {
            return importSTB(memoryData, memorySize, options, outMetaData, outData);
            return false;
        }
        return importSTB(memoryData, memorySize, options, outMetaData, outData);
    }


    // -----------------------------------------------------------------------------
    // Импорт напрямую из RGBA-памяти
    // -----------------------------------------------------------------------------
    bool TextureImporter::importFromRGBA(const uint8_t* rgbaPixels, int width, int height, const TextureImportOptions& options, format::TextureMetaData& outMetaData, std::vector<uint8_t>& outData) {
        if (!rgbaPixels || width <= 0 || height <= 0) return false;

        auto const mipCount = options.generateMips ?
            static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1 : 1u;

        outData.clear();
        std::vector<uint8_t> currentMipPixels(rgbaPixels, rgbaPixels + (static_cast<size_t>(width) * height * 4));

        int curW = width;
        int curH = height;

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

        outMetaData.width = width;
        outMetaData.height = height;
        outMetaData.mipCount = mipCount;
        outMetaData.numLayers = 1;
        outMetaData.isCubemap = 0;
        outMetaData.format = (options.format == TextureFormat::BC7_SRGB) ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK;

        return true;
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
        uint8_t* rawPixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &comp, 4); // Всегда RGBA
        if (!rawPixels) return false;

        auto const mipCount = options.generateMips ?
            static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1 : 1;

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
        outMetaData.format = (options.format == TextureFormat::BC7_SRGB) ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK; // Vulkan BC7/BC5

        return true;
    }

    // -----------------------------------------------------------------------------
    // Утилита: упаковать раздельные AO / Roughness / Metalness в одну ORM текстуру
    // R = Occlusion, G = Roughness, B = Metalness, A = 255
    // Принимает пути к файлам — любой из них может быть отсутствующим (std::nullopt).
    // Если roughnessIsGloss == true, входной roughness интерпретируется как glossiness и будет инвертирован.
    // -----------------------------------------------------------------------------
    void TextureImporter::packORMFromFiles(
        const std::optional<std::string>& occlusionPath,
        const std::optional<std::string>& roughnessPath,
        const std::optional<std::string>& metalnessPath,
        const TextureImportOptions& options,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData,
        bool roughnessIsGloss
    ) {
        // Загрузим изображения (если присутствуют), сохраняя их в RGBA8 буферы
        int wA=0,hA=0, cA=0;
        int wR=0,hR=0, cR=0;
        int wM=0,hM=0, cM=0;
        uint8_t* dataA = nullptr;
        uint8_t* dataR = nullptr;
        uint8_t* dataM = nullptr;

        try {
            if (occlusionPath && !occlusionPath->empty()) dataA = stbi_load(occlusionPath->c_str(), &wA, &hA, &cA, 4);
            if (roughnessPath && !roughnessPath->empty()) dataR = stbi_load(roughnessPath->c_str(), &wR, &hR, &cR, 4);
            if (metalnessPath && !metalnessPath->empty()) dataM = stbi_load(metalnessPath->c_str(), &wM, &hM, &cM, 4);
        } catch (...) {
            if (dataA) stbi_image_free(dataA);
            if (dataR) stbi_image_free(dataR);
            if (dataM) stbi_image_free(dataM);
        }

        // Выберем целевой размер: используем максимум по ширине/высоте среди доступных карт, либо 1x1
        int targetW = 1, targetH = 1;
        if (dataA) { targetW = std::max(targetW, wA); targetH = std::max(targetH, hA); }
        if (dataR) { targetW = std::max(targetW, wR); targetH = std::max(targetH, hR); }
        if (dataM) { targetW = std::max(targetW, wM); targetH = std::max(targetH, hM); }

        // Создаем рабочие буферы в RGBA8 одинакового размера
        std::vector<uint8_t> bufA(targetW * targetH * 4);
        std::vector<uint8_t> bufR(targetW * targetH * 4);
        std::vector<uint8_t> bufM(targetW * targetH * 4);

        // Если нет данных — заполним дефолтами (AO=255, Rough=255, Metal=0)
        if (!dataA) {
            for (unsigned char & i : bufA) i = 255;
        } else {
            if (wA == targetW && hA == targetH) {
                std::memcpy(bufA.data(), dataA, targetW * targetH * 4);
            } else {
                stbir_resize_uint8_linear(dataA, wA, hA, 0, bufA.data(), targetW, targetH, 0, STBIR_RGBA);
            }
        }

        if (!dataR) {
            for (unsigned char & i : bufR) i = 255;
        } else {
            if (wR == targetW && hR == targetH) {
                std::memcpy(bufR.data(), dataR, targetW * targetH * 4);
            } else {
                stbir_resize_uint8_linear(dataR, wR, hR, 0, bufR.data(), targetW, targetH, 0, STBIR_RGBA);
            }
        }

        if (!dataM) {
            for (unsigned char & i : bufM) i = 0;
        } else {
            if (wM == targetW && hM == targetH) {
                std::memcpy(bufM.data(), dataM, targetW * targetH * 4);
            } else {
                stbir_resize_uint8_linear(dataM, wM, hM, 0, bufM.data(), targetW, targetH, 0, STBIR_RGBA);
            }
        }

        // Освободим исходники
        if (dataA) stbi_image_free(dataA);
        if (dataR) stbi_image_free(dataR);
        if (dataM) stbi_image_free(dataM);

        // Соберем итоговый RGBA буфер: R = occlusion (use red channel of bufA), G = roughness (red), B = metalness (red), A = 255
        std::vector<uint8_t> finalRGBA(targetW * targetH * 4);
        for (int y = 0; y < targetH; ++y) {
            for (int x = 0; x < targetW; ++x) {
                size_t idx = (static_cast<size_t>(y) * targetW + x) * 4;
                uint8_t ao = bufA[idx + 0];
                uint8_t rough = bufR[idx + 0];
                uint8_t metal = bufM[idx + 0];
                if (roughnessIsGloss) rough = static_cast<uint8_t>(255 - rough);
                finalRGBA[idx + 0] = ao;
                finalRGBA[idx + 1] = rough;
                finalRGBA[idx + 2] = metal;
                finalRGBA[idx + 3] = 255;
            }
        }

        // Теперь компрессим и генерируем мипы аналогично importSTB
        outData.clear();
        std::vector<uint8_t> currentMipPixels = std::move(finalRGBA);
        int curW = targetW;
        int curH = targetH;

        uint32_t mipCount = options.generateMips ? static_cast<uint32_t>(std::floor(std::log2(std::max(curW, curH)))) + 1u : 1u;

        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            std::vector<uint8_t> compressedLevel;
            if (options.format == TextureFormat::BC7_SRGB) {
                compressedLevel = compressToBC7(currentMipPixels.data(), curW, curH);
            } else if (options.format == TextureFormat::BC5_UNORM) {
                renormalizeNormalMap(currentMipPixels, curW, curH); // harmless for ORM but keep parity
                compressedLevel = compressToBC5(currentMipPixels.data(), curW, curH);
            }

            outData.insert(outData.end(), compressedLevel.begin(), compressedLevel.end());

            if (mip < mipCount - 1) {
                int nextW = std::max(1, curW / 2);
                int nextH = std::max(1, curH / 2);
                std::vector<uint8_t> nextMip(nextW * nextH * 4);
                stbir_resize_uint8_linear(currentMipPixels.data(), curW, curH, 0, nextMip.data(), nextW, nextH, 0, STBIR_RGBA);
                currentMipPixels = std::move(nextMip);
                curW = nextW; curH = nextH;
            }
        }

        outMetaData.width = targetW;
        outMetaData.height = targetH;
        outMetaData.mipCount = mipCount;
        outMetaData.numLayers = 1;
        outMetaData.isCubemap = 0;
        outMetaData.format = (options.format == TextureFormat::BC7_SRGB) ? 145u : 141u;
    }

    // -----------------------------------------------------------------------------
    // Упаковка ORM из памяти (например, из встроенных Assimp текстур)
    // Формат и семантика такие же, как у packORMFromFiles.
    // -----------------------------------------------------------------------------
    void TextureImporter::packORMFromMemory(
        const uint8_t* occlusionData, size_t occlusionSize,
        const uint8_t* roughnessData, size_t roughnessSize,
        const uint8_t* metalnessData, size_t metalnessSize,
        const TextureImportOptions& options,
        format::TextureMetaData& outMetaData,
        std::vector<uint8_t>& outData,
        bool roughnessIsGloss
    ) {
        int wA=0,hA=0,cA=0;
        int wR=0,hR=0,cR=0;
        int wM=0,hM=0,cM=0;
        uint8_t* dataA = nullptr;
        uint8_t* dataR = nullptr;
        uint8_t* dataM = nullptr;

        if (occlusionData && occlusionSize > 0) dataA = stbi_load_from_memory(occlusionData, static_cast<int>(occlusionSize), &wA, &hA, &cA, 4);
        if (roughnessData && roughnessSize > 0) dataR = stbi_load_from_memory(roughnessData, static_cast<int>(roughnessSize), &wR, &hR, &cR, 4);
        if (metalnessData && metalnessSize > 0) dataM = stbi_load_from_memory(metalnessData, static_cast<int>(metalnessSize), &wM, &hM, &cM, 4);

        // Выберем целевой размер
        int targetW = 1, targetH = 1;
        if (dataA) { targetW = std::max(targetW, wA); targetH = std::max(targetH, hA); }
        if (dataR) { targetW = std::max(targetW, wR); targetH = std::max(targetH, hR); }
        if (dataM) { targetW = std::max(targetW, wM); targetH = std::max(targetH, hM); }

        std::vector<uint8_t> bufA(targetW * targetH * 4);
        std::vector<uint8_t> bufR(targetW * targetH * 4);
        std::vector<uint8_t> bufM(targetW * targetH * 4);

        if (!dataA) {
            std::ranges::fill(bufA, 255);
        } else {
            if (wA == targetW && hA == targetH) std::memcpy(bufA.data(), dataA, targetW * targetH * 4);
            else stbir_resize_uint8_linear(dataA, wA, hA, 0, bufA.data(), targetW, targetH, 0, STBIR_RGBA);
        }

        if (!dataR) {
            std::ranges::fill(bufR, 255);
        } else {
            if (wR == targetW && hR == targetH) std::memcpy(bufR.data(), dataR, targetW * targetH * 4);
            else stbir_resize_uint8_linear(dataR, wR, hR, 0, bufR.data(), targetW, targetH, 0, STBIR_RGBA);
        }

        if (!dataM) {
            std::ranges::fill(bufM, 0);
        } else {
            if (wM == targetW && hM == targetH) std::memcpy(bufM.data(), dataM, targetW * targetH * 4);
            else stbir_resize_uint8_linear(dataM, wM, hM, 0, bufM.data(), targetW, targetH, 0, STBIR_RGBA);
        }

        if (dataA) stbi_image_free(dataA);
        if (dataR) stbi_image_free(dataR);
        if (dataM) stbi_image_free(dataM);

        // Собираем финальный RGBA
        std::vector<uint8_t> finalRGBA(targetW * targetH * 4);
        for (int y = 0; y < targetH; ++y) {
            for (int x = 0; x < targetW; ++x) {
                size_t idx = (static_cast<size_t>(y) * targetW + x) * 4;
                uint8_t ao = bufA[idx + 0];
                uint8_t rough = bufR[idx + 0];
                uint8_t metal = bufM[idx + 0];
                if (roughnessIsGloss) rough = static_cast<uint8_t>(255 - rough);
                finalRGBA[idx + 0] = ao;
                finalRGBA[idx + 1] = rough;
                finalRGBA[idx + 2] = metal;
                finalRGBA[idx + 3] = 255;
            }
        }

        // Компрессия и мипы — переиспользуем реализацию из packORMFromFiles
        outData.clear();
        std::vector<uint8_t> currentMipPixels = std::move(finalRGBA);
        int curW = targetW;
        int curH = targetH;

        uint32_t mipCount = options.generateMips ? static_cast<uint32_t>(std::floor(std::log2(std::max(curW, curH)))) + 1u : 1u;

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
                std::vector<uint8_t> nextMip(nextW * nextH * 4);
                stbir_resize_uint8_linear(currentMipPixels.data(), curW, curH, 0, nextMip.data(), nextW, nextH, 0, STBIR_RGBA);
                currentMipPixels = std::move(nextMip);
                curW = nextW; curH = nextH;
            }
        }

        outMetaData.width = targetW;
        outMetaData.height = targetH;
        outMetaData.mipCount = mipCount;
        outMetaData.numLayers = 1;
        outMetaData.isCubemap = 0;
        outMetaData.format = (options.format == TextureFormat::BC7_SRGB) ? 145u : 141u;
    }

    std::vector<uint8_t> TextureImporter::compressToBC7(const uint8_t* rgbaPixels, int width, int height) {
        int const blocksX = (width + 3) / 4;
        int const blocksY = (height + 3) / 4;

        // BC7 занимает ровно 16 байт на блок 4x4 пикселя
        std::vector<uint8_t> outData(static_cast<size_t>(blocksX) * blocksY * 16);

        bc7_enc_settings settings{};
        // ИСПРАВЛЕНО: используем точное имя функции из заголовка Intel ISPC
        GetProfile_basic(&settings);

        size_t const rowStrideInBytes = static_cast<size_t>(width) * 4;

        #pragma omp parallel for default(none) \
        shared(blocksY, blocksX, width, rgbaPixels, outData, settings, rowStrideInBytes) \
        schedule(dynamic, 1)
        for (int by = 0; by < blocksY; ++by) {
            // Указатель на начало строки блоков (каждая полоса высотой ровно в 1 блок = 4 пикселя)
            const uint8_t* srcRowPtr = rgbaPixels + (static_cast<size_t>(by) * 4) * rowStrideInBytes;

            // Настраиваем под-поверхность для Intel ядра
            rgba_surface surface{};
            surface.ptr = const_cast<uint8_t*>(srcRowPtr);
            surface.width = width;
            surface.height = 4; // Высота полосы ВСЕГДА 4 пикселя (1 блок)
            surface.stride = static_cast<int>(rowStrideInBytes);

            // Указатель на место в общем векторе, куда текущий поток запишет сжатую строку блоков
            uint8_t* dstBlockPtr = outData.data() + (static_cast<size_t>(by) * blocksX * 16);

            // Тяжелое SIMD (AVX2) сжатие строки блоков силами Intel ISPC
            CompressBlocksBC7(&surface, dstBlockPtr, &settings);
        }

        return outData;
    }



    std::vector<uint8_t> TextureImporter::compressToBC5(const uint8_t* rgbaPixels, int width, int height) {
        int const blocksX = (width + 3) / 4;
        int const blocksY = (height + 3) / 4;

        // Формат BC5 занимает ровно 16 байт на блок 4x4 пикселя
        std::vector<uint8_t> outData(static_cast<size_t>(blocksX) * blocksY * 16);

        size_t const rowStrideInBytes = static_cast<size_t>(width) * 4;

        #pragma omp parallel for default(none) \
        shared(blocksY, blocksX, width, rgbaPixels, outData, rowStrideInBytes) \
        schedule(dynamic, 1)
        for (int by = 0; by < blocksY; ++by) {
            // Вычисляем указатель на начало текущей строки блоков (высотой 4 пикселя)
            const uint8_t* srcRowPtr = rgbaPixels + (static_cast<size_t>(by) * 4) * rowStrideInBytes;

            // Инициализируем структуру поверхности для Intel ядра
            rgba_surface surface{};
            surface.ptr = const_cast<uint8_t*>(srcRowPtr);
            surface.width = width;
            surface.height = 4; // Высота полосы фиксирована под 1 блок (4 пикселя)
            surface.stride = static_cast<int>(rowStrideInBytes);

            // Рассчитываем смещение, куда текущий поток запишет сжатые блоки текущей строки
            uint8_t* dstBlockPtr = outData.data() + (static_cast<size_t>(by) * blocksX * 16);

            // Запуск векторизованного (AVX2) сжатия карты нормалей силами Intel ISPC
            CompressBlocksBC5(&surface, dstBlockPtr);
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

        switch (ddsFormat) {
            case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm:
                vkFormat = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB:
                vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm:
                vkFormat = VK_FORMAT_BC2_UNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB:
                vkFormat = VK_FORMAT_BC2_SRGB_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm:
                vkFormat = VK_FORMAT_BC3_UNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB:
                vkFormat = VK_FORMAT_BC3_SRGB_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm:
                vkFormat = VK_FORMAT_BC4_UNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm:
                vkFormat = VK_FORMAT_BC4_SNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm:
                vkFormat = VK_FORMAT_BC5_UNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm:
                vkFormat = VK_FORMAT_BC5_SNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm:
                vkFormat = VK_FORMAT_BC7_UNORM_BLOCK;
                break;
            case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB:
                vkFormat = VK_FORMAT_BC7_SRGB_BLOCK;
                break;
            default:
                std::cerr << "[TextureImporter] DDS format not supported: " << static_cast<int>(ddsFormat) << std::endl;
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

} // namespace shuttle_engine::compiler
