#include "BlobLoader.hpp"
#include <fstream>
#include <algorithm>

namespace shuttle_engine {

    // =========================================================================
    // mip size calculation
    // =========================================================================

    uint32_t BlobSceneData::calcMipSize(uint32_t w, uint32_t h, uint32_t format_vk) {
        // VkFormat BC range starts at 131 (BC1_RGB_UNORM_BLOCK).
        // BC1 (131-134) and BC4 (139-140) use 8 bytes per 4x4 block; all others use 16.
        constexpr uint32_t VK_FORMAT_BC1_RGB_UNORM_BLOCK  = 131;
        constexpr uint32_t VK_FORMAT_BC1_RGBA_SRGB_BLOCK  = 134;
        constexpr uint32_t VK_FORMAT_BC4_UNORM_BLOCK      = 139;
        constexpr uint32_t VK_FORMAT_BC4_SNORM_BLOCK      = 140;

        if (format_vk >= VK_FORMAT_BC1_RGB_UNORM_BLOCK) {
            // Block-compressed format
            uint32_t blockBytes = 16;
            if ((format_vk <= VK_FORMAT_BC1_RGBA_SRGB_BLOCK) ||
                (format_vk == VK_FORMAT_BC4_UNORM_BLOCK || format_vk == VK_FORMAT_BC4_SNORM_BLOCK)) {
                blockBytes = 8;
            }
            uint32_t blocksX = (w + 3) / 4;
            uint32_t blocksY = (h + 3) / 4;
            return blocksX * blocksY * blockBytes;
        }

        // Uncompressed: assume 4 bytes per pixel (R8G8B8A8 family).
        return w * h * 4;
    }

    uint64_t BlobSceneData::calcTextureDataSize(const format::TextureMetaData& meta) {
        uint64_t total = 0;
        for (uint32_t mip = 0; mip < meta.mipCount; ++mip) {
            uint32_t w = std::max(1u, meta.width  >> mip);
            uint32_t h = std::max(1u, meta.height >> mip);
            total += calcMipSize(w, h, meta.format);
        }
        return total;
    }

    // =========================================================================
    // span accessors
    // =========================================================================

    std::span<const uint8_t> BlobSceneData::getTextureData(uint32_t texIdx) const {
        const auto& meta = textures[texIdx];
        uint64_t size = calcTextureDataSize(meta);
        return { fileData.data() + meta.textureOffset, size };
    }

    std::span<const uint8_t> BlobSceneData::getMeshPositionData(uint32_t meshIdx) const {
        uint64_t posAddr  = meshes[meshIdx].positionBufferAddress;
        uint64_t attrAddr = meshes[meshIdx].attributeBufferAddress;
        return { fileData.data() + posAddr, attrAddr - posAddr };
    }

    std::span<const uint8_t> BlobSceneData::getMeshAttributeData(uint32_t meshIdx) const {
        uint64_t attrAddr = meshes[meshIdx].attributeBufferAddress;
        uint64_t idxAddr  = meshes[meshIdx].indexBufferAddress;
        return { fileData.data() + attrAddr, idxAddr - attrAddr };
    }

    std::span<const uint8_t> BlobSceneData::getMeshIndexData(uint32_t meshIdx) const {
        uint64_t idxAddr = meshes[meshIdx].indexBufferAddress;
        // Sum indices across ALL meshes — the blob stores a single merged index buffer.
        uint64_t totalIndices = 0;
        for (const auto& mh : meshes) {
            for (uint32_t l = 0; l < mh.lodCount; ++l) {
                totalIndices += mh.lods[l].indexCount;
            }
        }
        return { fileData.data() + idxAddr, totalIndices * sizeof(uint32_t) };
    }

    // =========================================================================
    // BlobLoader::load
    // =========================================================================

    template<typename T>
    static std::span<const T> makeSpan(const std::vector<uint8_t>& data, uint64_t offset, uint32_t count) {
        if (count == 0) return {};
        return { reinterpret_cast<const T*>(data.data() + offset), count };
    }

    BlobSceneData BlobLoader::load(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("BlobLoader: cannot open file: " + path);
        }

        auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0);

        BlobSceneData result;
        result.fileData.resize(fileSize);
        file.read(reinterpret_cast<char*>(result.fileData.data()), static_cast<std::streamsize>(fileSize));
        file.close();

        if (fileSize < sizeof(format::BlobHeader)) {
            throw std::runtime_error("BlobLoader: file too small to contain a valid header: " + path);
        }

        result.header = reinterpret_cast<const format::BlobHeader*>(result.fileData.data());

        if (result.header->magic[0] != 'B' || result.header->magic[1] != 'L' ||
            result.header->magic[2] != 'O' || result.header->magic[3] != 'B') {
            throw std::runtime_error("BlobLoader: invalid magic bytes in file: " + path);
        }

        const auto& h = *result.header;
        result.textures   = makeSpan<format::TextureMetaData> (result.fileData, h.textureTableOffset,        h.textureCount);
        result.materials  = makeSpan<format::MaterialInfo>    (result.fileData, h.materialTableOffset,       h.materialCount);
        result.meshes     = makeSpan<format::MeshHeader>      (result.fileData, h.meshTableOffset,           h.meshCount);
        result.sceneNodes = makeSpan<format::SceneNode>       (result.fileData, h.sceneGraphOffset,          h.sceneNodeCount);
        result.nodeLevels = makeSpan<format::NodeLevelRange>  (result.fileData, h.nodeLevelRangeTableOffset, h.nodeLevelRangeCount);
        result.dirLights  = makeSpan<format::DirectionalLight>(result.fileData, h.dirLightTableOffset,       h.dirLightCount);
        result.pointLights= makeSpan<format::PointLight>      (result.fileData, h.pointLightTableOffset,     h.pointLightCount);
        result.spotLights = makeSpan<format::SpotLight>       (result.fileData, h.spotLightTableOffset,      h.spotLightCount);

        return result;
    }

} // namespace shuttle_engine
