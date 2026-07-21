#pragma once
#include <vector>
#include <span>
#include <string>
#include <stdexcept>
#include "../../AssetProcessor/include/BlobLayout.hpp"

namespace shuttle_engine {

    struct BlobSceneData {
        std::vector<uint8_t> fileData; // owns the raw file bytes

        const format::BlobHeader*              header     = nullptr;
        std::span<const format::TextureMetaData>  textures;
        std::span<const format::MaterialInfo>     materials;
        std::span<const format::MeshHeader>       meshes;
        std::span<const format::SceneNode>        sceneNodes;
        std::span<const format::NodeLevelRange>   nodeLevels;
        std::span<const format::DirectionalLight> dirLights;
        std::span<const format::PointLight>       pointLights;
        std::span<const format::SpotLight>        spotLights;

        // Returns all mip data for texture texIdx (using textureOffset as absolute file offset).
        [[nodiscard]] std::span<const uint8_t> getTextureData(uint32_t texIdx) const;

        // Returns combined position buffer span (call with meshIdx=0 for the full buffer).
        // positionBufferAddress and attributeBufferAddress are absolute file offsets.
        [[nodiscard]] std::span<const uint8_t> getMeshPositionData(uint32_t meshIdx) const;
        [[nodiscard]] std::span<const uint8_t> getMeshAttributeData(uint32_t meshIdx) const;
        [[nodiscard]] std::span<const uint8_t> getMeshIndexData(uint32_t meshIdx) const;

        // Total byte size of all mips for a texture.
        static uint64_t calcTextureDataSize(const format::TextureMetaData& meta);

        // Byte size of one mip level at (w x h) for the given VkFormat enum value.
        static uint32_t calcMipSize(uint32_t w, uint32_t h, uint32_t format_vk);
    };

    class BlobLoader {
    public:
        static BlobSceneData load(const std::string& path);
    };

} // namespace shuttle_engine
