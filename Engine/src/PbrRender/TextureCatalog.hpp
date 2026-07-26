//
// Created by Shagu on 22.07.2026.
//

#ifndef SHUTTLEENGINE_TEXTURECATALOG_HPP
#define SHUTTLEENGINE_TEXTURECATALOG_HPP
#include <vector>
#include "TextureCatalog.hpp"

namespace ShuttleEngine::render{
    class TextureCatalog {
    public:
        TextureCatalog(
            vk::ImageView albedoFallbackTexture,
            vk::ImageView normalFallbackTexture,
            vk::ImageView ormFallbackTexture,
            vk::ImageView emissionFallbackTexture,
            uint32_t userTextureCount
        ) {
            descriptorImageInfos.reserve(userTextureCount + 4);
            descriptorImageInfos.emplace_back(VK_NULL_HANDLE, albedoFallbackTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
            descriptorImageInfos.emplace_back(VK_NULL_HANDLE, normalFallbackTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
            descriptorImageInfos.emplace_back(VK_NULL_HANDLE, ormFallbackTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
            descriptorImageInfos.emplace_back(VK_NULL_HANDLE, emissionFallbackTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        void addTextureView(vk::ImageView textureView) {
            descriptorImageInfos.emplace_back( VK_NULL_HANDLE, textureView, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        void writeDescriptors(
            vk::Device device,
            vk::DescriptorSet descriptorSet,
            uint32_t binding
        ) const {
            vk::WriteDescriptorSet writeDescriptorSet{
                .dstSet = descriptorSet,
                .dstBinding = binding,
                .dstArrayElement = 0U,
                .descriptorCount = static_cast<uint32_t>(descriptorImageInfos.size()),
                .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = descriptorImageInfos.data()
            };
            device.updateDescriptorSets({writeDescriptorSet}, {});
        }

        [[nodiscard]] uint32_t size() const noexcept {
            return static_cast<uint32_t>(descriptorImageInfos.size());
        }

        [[nodiscard]] vk::DescriptorImageInfo const& get(uint32_t index) const {
            return descriptorImageInfos[index];
        }
    private:
        std::vector<vk::DescriptorImageInfo> descriptorImageInfos;
    };
}


#endif //SHUTTLEENGINE_TEXTURECATALOG_HPP
