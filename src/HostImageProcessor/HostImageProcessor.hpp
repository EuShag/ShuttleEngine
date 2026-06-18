//
// Created by Shagu on 28.05.2026.
//

#ifndef HELLOTRIANGLE_HOSTIMAGEPROCESSOR_HPP
#define HELLOTRIANGLE_HOSTIMAGEPROCESSOR_HPP
#include "HostRenderData/HostRenderData.hpp"

namespace shuttle_engine::resources {
    enum class MipFilter { Box, NormalMap };

    class TextureProcessor {
    public:
        static void prepareImageData(HostImageData& image, MipFilter filter);

    private:
        static std::vector<MipInfo> calculateMipChain(uint32_t w, uint32_t h, uint32_t const ch) {
            std::vector<MipInfo> chain;
            size_t offset = 0;
            while (true) {
                size_t const size = w * h * ch;
                chain.push_back({w, h, offset, size});
                offset += size;
                if (w == 1 && h == 1) break;
                w = std::max(1u, w / 2); h = std::max(1u, h / 2);
            }
            return chain;
        }

        static void generateLevel(HostImageData& img, const MipInfo& prev, const MipInfo& curr, MipFilter const filter) {
            for (uint32_t y = 0; y < curr.height; ++y) {
                for (uint32_t x = 0; x < curr.width; ++x) {
                    const uint8_t* p[4];
                    auto getPtr = [&](uint32_t px, uint32_t py) {
                        px = std::min(px, prev.width - 1); py = std::min(py, prev.height - 1);
                        return &img.data[prev.offset + (py * prev.width + px) * 4];
                    };
                    p[0] = getPtr(x * 2, y * 2); p[1] = getPtr(x * 2 + 1, y * 2);
                    p[2] = getPtr(x * 2, y * 2 + 1); p[3] = getPtr(x * 2 + 1, y * 2 + 1);

                    uint8_t* dst = &img.data[curr.offset + (y * curr.width + x) * 4];
                    if (filter == MipFilter::Box) {
                        for (int c = 0; c < 4; ++c) dst[c] = (p[0][c] + p[1][c] + p[2][c] + p[3][c]) / 4;
                    } else {
                        glm::vec3 sum(0.0f);
                        for (auto & i : p) sum += (glm::vec3(i[0], i[1], i[2]) / 255.0f * 2.0f - 1.0f);
                        glm::vec3 const res = (glm::normalize(sum) * 0.5f + 0.5f) * 255.0f;
                        dst[0] = static_cast<uint8_t>(res.x); dst[1] = static_cast<uint8_t>(res.y); dst[2] = static_cast<uint8_t>(res.z); dst[3] = 255;
                    }
                }
            }
        }
    };

    inline void TextureProcessor::prepareImageData(HostImageData &image, MipFilter filter) {
        // 1. Вычисляем метаданные всех уровней
        image.mipChain = calculateMipChain(image.width, image.height, 4);

        // 2. Выделяем место под оригинал + все мипы
        size_t totalBytes = 0;
        for (const auto& m : image.mipChain) totalBytes += m.size;

        // Предполагаем, что original data уже в image.data, копируем её в начало нового буфера
        std::vector<uint8_t> allData(totalBytes);
        std::copy(image.data.begin(), image.data.end(), allData.begin());
        image.data = std::move(allData);

        // 3. Генерируем мипы прямо в буфере
        for (size_t i = 1; i < image.mipChain.size(); ++i) {
            generateLevel(image, image.mipChain[i-1], image.mipChain[i], filter);
        }
    }

    inline std::vector<vk::BufferImageCopy> createRegions(const HostImageData& img) {
        std::vector<vk::BufferImageCopy> regions;
        for (uint32_t i = 0; i < img.mipChain.size(); ++i) {
            vk::BufferImageCopy r{};
            r.bufferOffset = img.mipChain[i].offset;
            r.imageSubresource = {vk::ImageAspectFlagBits::eColor, i, 0, 1};
            r.imageExtent = vk::Extent3D{.width = img.mipChain[i].width, .height = img.mipChain[i].height, .depth = 1};
            regions.push_back(r);
        }
        return regions;
    }

} // shuttle_engine

#endif //HELLOTRIANGLE_HOSTIMAGEPROCESSOR_HPP
