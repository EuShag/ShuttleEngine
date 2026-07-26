#include "CompiledSceneBlobWriter.hpp"

#include <Assets/Core/BlobWriter.hpp>
#include <Assets/Formats/Texture.hpp>

#include <span>
#include <vector>

namespace shuttle::assets::scene_compiler
{
    namespace {
        using namespace shuttle::assets;

        template<typename T>
        void addTypedSection(
            core::BlobWriter& writer,
            core::BlobSectionType type,
            const std::vector<T>& data)
        {
            if (data.empty())
            {
                return;
            }

            writer.addTypedSection(
                type,
                std::span<const T>(
                    data.data(),
                    data.size()));
        }

        struct TextureSerializationData
        {
            std::vector<formats::texture::TextureMetadata>
                metadata;

            std::vector<formats::texture::TextureMipMetadata>
                mipMetadata;

            std::vector<uint8_t>
                textureData;
        };

        TextureSerializationData buildTextureData(
    const CompiledScene& scene)
        {
            TextureSerializationData result;

            uint64_t globalDataOffset = 0;
            uint64_t globalMipOffset = 0;

            result.metadata.reserve(
                scene.textures.size());

            for (const auto& texture : scene.textures)
            {
                auto metadata =
                    texture.metadata;

                metadata.mipTableOffset =
                    globalMipOffset;

                result.metadata.push_back(
                    metadata);

                for (auto mip : texture.mipMetadata)
                {
                    mip.dataOffset +=
                        globalDataOffset;

                    result.mipMetadata.push_back(
                        mip);
                }

                globalMipOffset +=
                    static_cast<uint64_t>(
                        texture.mipMetadata.size()) *
                    sizeof(formats::texture::TextureMipMetadata);

                result.textureData.insert(
                    result.textureData.end(),
                    texture.data.begin(),
                    texture.data.end());

                globalDataOffset +=
                    texture.data.size();
            }

            return result;
        }
    }

    bool CompiledSceneBlobWriter::write(
        const CompiledScene& scene,
        const std::filesystem::path& outputPath)
    {
        core::BlobWriter writer;

        //
        // textures
        //

        {
            TextureSerializationData textures =
                buildTextureData(
                    scene);

            addTypedSection(
                writer,
                core::BlobSectionType::TextureMetadata,
                textures.metadata);

            addTypedSection(
                writer,
                core::BlobSectionType::TextureMipMetadata,
                textures.mipMetadata);

            if (!textures.textureData.empty())
            {
                writer.addSection(
                    core::BlobSectionType::TextureData,
                    std::span<const uint8_t>(
                        textures.textureData.data(),
                        textures.textureData.size()));
            }
        }

        //
        // materials
        //

        addTypedSection(
            writer,
            core::BlobSectionType::GpuMaterials,
            scene.materials);

        //
        // geometry
        //

        addTypedSection(
            writer,
            core::BlobSectionType::PositionMegabuffer,
            scene.positions);

        addTypedSection(
            writer,
            core::BlobSectionType::AttributeMegabuffer,
            scene.attributes);

        addTypedSection(
            writer,
            core::BlobSectionType::SkinMegabuffer,
            scene.skins);

        addTypedSection(
            writer,
            core::BlobSectionType::IndexMegabuffer,
            scene.indices);

        addTypedSection(
            writer,
            core::BlobSectionType::GpuMeshes,
            scene.meshes);

        //
        // scene graph
        //

        addTypedSection(
            writer,
            core::BlobSectionType::GpuSceneNodes,
            scene.nodes);

        addTypedSection(
            writer,
            core::BlobSectionType::GpuNodeLevels,
            scene.levels);

        addTypedSection(
            writer,
            core::BlobSectionType::GpuDrawableObjects,
            scene.drawableObjects);

        //
        // animation
        //

        addTypedSection(
            writer,
            core::BlobSectionType::Skeletons,
            scene.skeletons);

        addTypedSection(
            writer,
            core::BlobSectionType::Bones,
            scene.bones);

        addTypedSection(
            writer,
            core::BlobSectionType::BoneChannels,
            scene.boneChannels);

        addTypedSection(
            writer,
            core::BlobSectionType::KeyframeTimes,
            scene.keyframeTimes);

        addTypedSection(
            writer,
            core::BlobSectionType::KeyframeValues,
            scene.keyframeValues);

        //
        // future animation sections
        //

        if (!scene.morphTargets.empty())
        {
            writer.addTypedSection(
                core::BlobSectionType::Custom,
                std::span<const formats::animation::MorphTarget>(
                    scene.morphTargets.data(),
                    scene.morphTargets.size()));
        }

        if (!scene.morphVertexDeltas.empty())
        {
            writer.addTypedSection(
                core::BlobSectionType::Custom,
                std::span<const formats::animation::MorphVertexDelta>(
                    scene.morphVertexDeltas.data(),
                    scene.morphVertexDeltas.size()));
        }

        if (!scene.morphChannels.empty())
        {
            writer.addTypedSection(
                core::BlobSectionType::Custom,
                std::span<const formats::animation::MorphChannel>(
                    scene.morphChannels.data(),
                    scene.morphChannels.size()));
        }

        if (!scene.materialProperties.empty())
        {
            writer.addTypedSection(
                core::BlobSectionType::Custom,
                std::span<const formats::animation::MaterialProperty>(
                    scene.materialProperties.data(),
                    scene.materialProperties.size()));
        }

        if (!scene.materialChannels.empty())
        {
            writer.addTypedSection(
                core::BlobSectionType::Custom,
                std::span<const formats::animation::MaterialChannel>(
                    scene.materialChannels.data(),
                    scene.materialChannels.size()));
        }

        return writer.write(
            outputPath);
    }
}