#include "CompiledEnvironmentBlobWriter.hpp"

#include <Assets/Core/BlobWriter.hpp>
#include <Assets/Formats/Texture.hpp>

#include <span>
#include <vector>

namespace shuttle::assets::environment_compiler
{
    namespace
    {
        struct EnvironmentTextureSerializationData
        {
            std::vector<
                formats::texture::TextureMetadata>
                metadata;

            std::vector<
                formats::texture::TextureMipMetadata>
                mipMetadata;

            std::vector<uint8_t>
                data;
        };

        [[nodiscard]]
        EnvironmentTextureSerializationData buildTextureData(
            const CompiledEnvironment& environment)
        {
            EnvironmentTextureSerializationData result;

            result.metadata.reserve(
                environment.textures.size());

            uint64_t globalMipOffset = 0;
            uint64_t globalDataOffset = 0;

            for (const auto& texture :
                 environment.textures)
            {
                auto metadata =
                    texture.metadata;

                metadata.mipTableOffset =
                    globalMipOffset;

                result.metadata.push_back(
                    metadata);

                for (auto mip :
                     texture.mipMetadata)
                {
                    mip.dataOffset +=
                        globalDataOffset;

                    result.mipMetadata.push_back(
                        mip);
                }

                globalMipOffset +=
                    static_cast<uint64_t>(
                        texture.mipMetadata.size()) *
                    sizeof(
                        formats::texture::TextureMipMetadata);

                result.data.insert(
                    result.data.end(),
                    texture.data.begin(),
                    texture.data.end());

                globalDataOffset +=
                    texture.data.size();
            }

            return result;
        }

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
    }

    bool CompiledEnvironmentBlobWriter::write(
        const CompiledEnvironment& environment,
        const std::filesystem::path& outputPath)
    {
        core::BlobWriter writer;

        //
        // environment info
        //

        writer.addTypedSection(
            core::BlobSectionType::EnvironmentInfo,
            std::span<
                const formats::environment::EnvironmentInfo>(
                &environment.info,
                1));

        //
        // textures
        //

        const auto textures =
            buildTextureData(
                environment);

        addTypedSection(
            writer,
            core::BlobSectionType::
                EnvironmentTextureMetadata,
            textures.metadata);

        addTypedSection(
            writer,
            core::BlobSectionType::
                EnvironmentTextureMipMetadata,
            textures.mipMetadata);

        if (!textures.data.empty())
        {
            writer.addSection(
                core::BlobSectionType::
                    EnvironmentTextureData,

                std::span<const uint8_t>(
                    textures.data.data(),
                    textures.data.size()));
        }

        return writer.write(
            outputPath);
    }
}