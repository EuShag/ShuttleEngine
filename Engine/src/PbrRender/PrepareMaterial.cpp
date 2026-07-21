//
// Created by Shagu on 30.05.2026.
//
#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine {

    // =========================================================================
    // fillDescriptorSet — writes UBO + 5 sampled-image bindings.
    // =========================================================================
    void PbrRender::fillDescriptorSet(
        vk::Device device,
        vk::DescriptorSet materialSet,
        vk::Buffer propertiesUbo,
        std::array<vk::ImageView, 5> const& textureViews)
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = propertiesUbo,
            .offset = 0,
            .range  = sizeof(HostMaterialProperties)
        };

        std::array<vk::DescriptorImageInfo, 5> imgInfos{{
            { .sampler = nullptr, .imageView = textureViews[0], .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = textureViews[1], .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = textureViews[2], .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = textureViews[3], .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
            { .sampler = nullptr, .imageView = textureViews[4], .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal },
        }};

        std::array<vk::WriteDescriptorSet, 6> writes;
        writes[0] = vk::WriteDescriptorSet{
            .dstSet = materialSet, .dstBinding = 0, .dstArrayElement = 0,
            .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo
        };
        for (uint32_t i = 1; i < 6; ++i) {
            writes[i] = vk::WriteDescriptorSet{
                .dstSet = materialSet, .dstBinding = i, .dstArrayElement = 0,
                .descriptorCount = 1, .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = &imgInfos[i - 1]
            };
        }

        device.updateDescriptorSets(writes, {});
    }

} // namespace shuttle_engine
