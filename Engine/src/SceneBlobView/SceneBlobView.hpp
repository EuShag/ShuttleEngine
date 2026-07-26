#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <system_error>
#include <type_traits>

#include <mio/mio.hpp>

#include "asset_processor.hpp"

namespace shuttle_engine
{
    class SceneBlobView
    {
    public:
        SceneBlobView() = default;

        SceneBlobView(const SceneBlobView&) = delete;
        SceneBlobView& operator=(const SceneBlobView&) = delete;

        SceneBlobView(SceneBlobView&&) noexcept = default;
        SceneBlobView& operator=(SceneBlobView&&) noexcept = default;

        static SceneBlobView open(
            const std::filesystem::path& path)
        {
            SceneBlobView view{};

            std::error_code error;

            view.m_mapping.map(path.string(), error);

            if (error) {
                throw std::runtime_error("SceneBlobView: failed to map file: " + error.message());
            }

            if (view.m_mapping.empty()) {
                throw std::runtime_error("SceneBlobView: file is empty.");
            }

            if (view.m_mapping.size() < sizeof(format::SceneHeader)){
                throw std::runtime_error("SceneBlobView: file is smaller than SceneHeader.");
            }

            view.m_data = reinterpret_cast<const uint8_t*>(view.m_mapping.data());
            view.m_size = view.m_mapping.size();
            view.m_header = reinterpret_cast<const format::SceneHeader*>(view.m_data);

            if (std::memcmp(view.m_header->magic, "BLOB", 4) != 0) {
                throw std::runtime_error("SceneBlobView: invalid magic.");
            }

            if (view.m_header->totalFileSize == 0 || view.m_header->totalFileSize > view.m_size) {
                throw std::runtime_error("SceneBlobView: invalid declared file size.");
            }

            view.validateRange(
                view.m_header->bulkDataOffset,
                view.m_header->bulkDataSize);

            const auto& h = *view.m_header;

            view.m_textures = view.readSpan<format::TextureMetaData>(
                h.textureTableOffset,
                h.textureCount);

            view.m_textureMips = view.readSpan<format::TextureMipMetaData>(
                h.textureMipmapTableOffset,
                h.textureMipmapCount);

            view.m_materials = view.readSpan<format::MaterialInfo>(
                h.materialTableOffset,
                h.materialCount);

            view.m_meshes = view.readSpan<format::MeshHeader>(
                h.meshTableOffset,
                h.meshCount);

            view.m_sceneNodes = view.readSpan<format::SceneNode>(
                h.sceneGraphOffset,
                h.sceneNodeCount);

            view.m_nodeLevels = view.readSpan<format::NodeLevelRange>(
                h.nodeLevelRangeTableOffset,
                h.nodeLevelRangeCount);

            view.m_dirLights = view.readSpan<format::DirectionalLight>(
                h.dirLightTableOffset,
                h.dirLightCount);

            view.m_pointLights = view.readSpan<format::PointLight>(
                h.pointLightTableOffset,
                h.pointLightCount);

            view.m_spotLights = view.readSpan<format::SpotLight>(
                h.spotLightTableOffset,
                h.spotLightCount);

            view.validateOptionalRange(
                h.materialDataOffset,
                h.materialDataSize);

            view.validateOptionalRange(
                h.sceneNodeDataOffset,
                h.sceneNodeDataSize);

            view.validateOptionalRange(
                h.nodeLevelDataOffset,
                h.nodeLevelDataSize);

            return view;
        }

        [[nodiscard]] const format::SceneHeader& header() const noexcept {
            return *m_header;
        }

        [[nodiscard]] const uint8_t* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] uint64_t size() const noexcept {
            return m_size;
        }

        [[nodiscard]] std::span<const format::TextureMetaData> textures() const noexcept {
            return m_textures;
        }

        [[nodiscard]] std::span<const format::TextureMipMetaData> textureMips() const noexcept {
            return m_textureMips;
        }

        [[nodiscard]] std::span<const format::MaterialInfo> materials() const noexcept {
            return m_materials;
        }

        [[nodiscard]] std::span<const format::MeshHeader> meshes() const noexcept {
            return m_meshes;
        }

        [[nodiscard]] std::span<const format::SceneNode> sceneNodes() const noexcept {
            return m_sceneNodes;
        }

        [[nodiscard]] std::span<const format::NodeLevelRange> nodeLevels() const noexcept {
            return m_nodeLevels;
        }

        [[nodiscard]] std::span<const format::DirectionalLight> dirLights() const noexcept {
            return m_dirLights;
        }

        [[nodiscard]] std::span<const format::PointLight> pointLights() const noexcept {
            return m_pointLights;
        }

        [[nodiscard]] std::span<const format::SpotLight> spotLights() const noexcept {
            return m_spotLights;
        }

        [[nodiscard]] std::span<const uint8_t> bytesAt(uint64_t fileOffset, uint64_t byteSize) const {
            if (byteSize == 0) return {};
            validateRange(fileOffset, byteSize);
            return { m_data + fileOffset, byteSize };
        }

        [[nodiscard]] std::span<const uint8_t> bulkData() const{
            return bytesAt(m_header->bulkDataOffset, m_header->bulkDataSize);
        }

        [[nodiscard]] std::span<const uint8_t> materialGpuData() const {
            return optionalBytes(m_header->materialDataOffset, m_header->materialDataSize);
        }

        [[nodiscard]] std::span<const uint8_t> sceneNodeGpuData() const {
            return optionalBytes(m_header->sceneNodeDataOffset, m_header->sceneNodeDataSize);
        }

        [[nodiscard]] std::span<const uint8_t> nodeLevelGpuData() const{
            return optionalBytes(m_header->nodeLevelDataOffset, m_header->nodeLevelDataSize);
        }

        [[nodiscard]] std::span<const format::TextureMipMetaData> mipsForTexture(uint32_t textureIndex) const {
            if (textureIndex >= m_textures.size()) {
                throw std::runtime_error("SceneBlobView: texture index out of range.");
            }
            const auto& texture = m_textures[textureIndex];
            if (texture.mipCount == 0) return {};

            const uint64_t mipIndex = (texture.mipTableOffset - m_header->textureMipmapTableOffset)
                                      / sizeof(format::TextureMipMetaData);

            if (mipIndex + texture.mipCount > m_textureMips.size()) {
                throw std::runtime_error("SceneBlobView: invalid texture mip range.");
            }
            return { m_textureMips.data() + mipIndex, texture.mipCount };
        }

        [[nodiscard]] std::span<const uint8_t> mipBytes(
            const format::TextureMipMetaData& mip) const {
            return bytesAt(mip.dataOffset, mip.dataSize);
        }

    private:
        template <typename T> [[nodiscard]] std::span<const T> readSpan(
            uint64_t fileOffset,
            uint32_t count) const {
            static_assert( std::is_trivially_copyable_v<T>,
                "SceneBlobView only supports trivially copyable structs.");

            if (count == 0) return {};

            const uint64_t byteSize = static_cast<uint64_t>(count) * sizeof(T);
            validateRange(fileOffset, byteSize);
            return { reinterpret_cast<const T*>(m_data + fileOffset),count };
        }

        [[nodiscard]]
        std::span<const uint8_t> optionalBytes(
            uint64_t fileOffset,
            uint64_t byteSize) const {
            if (fileOffset == 0 || byteSize == 0) return {};
            return bytesAt(fileOffset, byteSize);
        }

        void validateOptionalRange(
            uint64_t fileOffset,
            uint64_t byteSize) const {

            if (fileOffset == 0 && byteSize == 0) return;
            validateRange(fileOffset, byteSize);
        }

        void validateRange(
            uint64_t fileOffset,
            uint64_t byteSize) const
        {
            if (fileOffset > m_size) throw std::runtime_error("SceneBlobView: file offset outside mapped file.");
            if (byteSize > m_size - fileOffset) throw std::runtime_error("SceneBlobView: byte range outside mapped file.");
        }

        mio::mmap_source m_mapping;

        const uint8_t* m_data = nullptr;
        uint64_t m_size = 0;

        const format::SceneHeader* m_header = nullptr;

        std::span<const format::TextureMetaData> m_textures;
        std::span<const format::TextureMipMetaData> m_textureMips;
        std::span<const format::MaterialInfo> m_materials;
        std::span<const format::MeshHeader> m_meshes;

        std::span<const format::SceneNode> m_sceneNodes;
        std::span<const format::NodeLevelRange> m_nodeLevels;

        std::span<const format::DirectionalLight> m_dirLights;
        std::span<const format::PointLight> m_pointLights;
        std::span<const format::SpotLight> m_spotLights;
    };
}